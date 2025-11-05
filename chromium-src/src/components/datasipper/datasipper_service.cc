// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/datasipper_service.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/time/time.h"
// Note: Cannot include chrome/browser headers here due to circular dependencies
// DataSipperNetworkBridge (in chrome/browser layer) will handle broadcasting
#include "chrome/common/chrome_features.h"
#include "components/datasipper/common/network_event.h"
#include "components/datasipper/datasipper_database.h"
#include "components/datasipper/extraction/extraction_result.h"
#include "components/datasipper/extraction/page_data_detector.h"
#include "components/datasipper/streaming/stream_registry.h"
#include "components/datasipper/streaming/network_stream_analyzer.h"
#include "components/datasipper/streaming/stream_types.h"
#include "components/datasipper/workflow/workflow_definition.h"
#include "components/datasipper/workflow/workflow_engine.h"
#include "components/datasipper/workflow/workflow_storage.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace datasipper {

namespace {
// Helper function to convert DataSourceType to string
const char* DataSourceTypeToString(DataSourceType type) {
  switch (type) {
    case DataSourceType::HTML_TABLE:
      return "HTML Table";
    case DataSourceType::JSON_OBJECT:
      return "JSON Object";
    case DataSourceType::JSON_LD:
      return "JSON-LD";
    case DataSourceType::CSV_DATA:
      return "CSV Data";
    case DataSourceType::FORM_DATA:
      return "Form Data";
    case DataSourceType::XML_DATA:
      return "XML Data";
    case DataSourceType::CUSTOM:
      return "Custom";
  }
  return "Unknown";
}

// Convert mojom NetworkRequestData to NetworkEvent for stream analysis
NetworkEvent ConvertToNetworkEvent(
    const side_panel::mojom::NetworkRequestDataPtr& request) {
  NetworkEvent event;

  event.id = request->request_id;
  event.url = GURL(request->url);
  event.method = request->method;
  event.status_code = request->status_code;
  event.timestamp = base::Time::FromMillisecondsSinceUnixEpoch(
      request->timestamp_ms);

  // Determine event type based on request type and status
  if (request->type == "websocket") {
    // TODO: Need more info to determine websocket message vs connect
    // For now, assume connect if we haven't seen this connection before
    event.type = EventType::WEBSOCKET_CONNECT;
    event.connection_id = request->request_id;  // Use request_id as connection_id
  } else {
    // Regular HTTP request/response
    if (request->status_code > 0) {
      event.type = EventType::HTTP_RESPONSE;
    } else {
      event.type = EventType::HTTP_REQUEST;
    }
  }

  return event;
}
}  // namespace

DataSipperService::TabTracker::TabTracker() = default;
DataSipperService::TabTracker::~TabTracker() = default;

DataSipperService::DataSipperService(const base::FilePath& database_path)
    : database_path_(database_path) {
  LOG(INFO) << "🔵 DataSipper: Service created at " << database_path_.value();
  LOG(INFO) << "🔵 DataSipper: Feature enabled = "
            << base::FeatureList::IsEnabled(features::kDataSipperEnabled);
  LOG(INFO) << "🔵 DataSipper: Network interception = "
            << base::FeatureList::IsEnabled(features::kDataSipperNetworkInterception);
}

DataSipperService::~DataSipperService() {
  Shutdown();
}

void DataSipperService::Shutdown() {
  LOG(INFO) << "🔵 DataSipper: Service shutting down";

  // Note: Network bridge unregistration moved to chrome/browser layer
  // to avoid circular dependencies

  // Clear all tab trackers
  tab_trackers_.clear();

  // Shutdown network stream analyzer
  if (network_stream_analyzer_) {
    network_stream_analyzer_->DisableAnalysis();
    network_stream_analyzer_.reset();
  }

  // Shutdown workflow system
  workflow_engine_.reset();
  workflow_storage_.reset();

  // Clear stream registry
  stream_registry_.reset();

  initialized_ = false;
}

bool DataSipperService::Initialize() {
  if (initialized_) {
    LOG(INFO) << "🔵 DataSipper: Already initialized";
    return true;
  }

  LOG(INFO) << "🔵 DataSipper: Initializing service...";

  if (!InitializeDatabase()) {
    LOG(ERROR) << "❌ DataSipper: Failed to initialize database";
    return false;
  }

  if (!InitializeStorageComponents()) {
    LOG(ERROR) << "❌ DataSipper: Failed to initialize storage components";
    return false;
  }

  if (!InitializeWorkflowSystem()) {
    LOG(ERROR) << "❌ DataSipper: Failed to initialize workflow system";
    return false;
  }

  initialized_ = true;

  // Note: Network bridge registration moved to chrome/browser layer
  // to avoid circular dependencies

  LOG(INFO) << "✅ DataSipper: Service fully initialized and ready!";
  return true;
}

bool DataSipperService::InitializeDatabase() {
  database_ = std::make_unique<DataSipperDatabase>();

  if (!database_->Init(database_path_)) {
    LOG(ERROR) << "Failed to initialize DataSipper database";
    database_.reset();
    return false;
  }

  LOG(INFO) << "✅ DataSipper: Database initialized successfully";
  return true;
}

bool DataSipperService::InitializeStorageComponents() {
  // Initialize stream registry
  stream_registry_ = std::make_unique<StreamRegistry>();
  LOG(INFO) << "✅ DataSipper: Stream registry initialized";

  // Initialize network stream analyzer (global across all tabs)
  network_stream_analyzer_ = std::make_unique<NetworkStreamAnalyzer>();

  // Enable network stream analysis with callback
  // Note: The callback doesn't need WebContents since network streams
  // are global. We'll use nullptr for the web_contents parameter.
  network_stream_analyzer_->EnableAnalysis(
      base::BindRepeating(&DataSipperService::OnStreamDetected,
                         base::Unretained(this),
                         nullptr));  // Network streams are not tab-specific

  LOG(INFO) << "✅ DataSipper: Network stream analyzer initialized";
  return true;
}

bool DataSipperService::InitializeWorkflowSystem() {
  if (!database_) {
    LOG(ERROR) << "Database must be initialized before workflow system";
    return false;
  }

  // Initialize workflow storage
  workflow_storage_ = std::make_unique<WorkflowStorage>(database_->db());
  if (!workflow_storage_->Initialize()) {
    LOG(ERROR) << "Failed to initialize workflow storage";
    return false;
  }

  // Initialize workflow engine
  // Note: We'll need a WebContents to get the URLLoaderFactory
  // For now, pass nullptr and we'll handle it when executing workflows
  workflow_engine_ = std::make_unique<WorkflowEngine>(
      workflow_storage_.get(), nullptr);

  LOG(INFO) << "✅ DataSipper: Workflow system initialized";
  return true;
}

// Tab lifecycle tracking
void DataSipperService::OnTabCreated(content::WebContents* web_contents) {
  RegisterTab(web_contents);
}

void DataSipperService::OnTabClosing(content::WebContents* web_contents) {
  UnregisterTab(web_contents);
}

void DataSipperService::RegisterTab(content::WebContents* web_contents) {
  if (!initialized_) {
    LOG(WARNING) << "DataSipper: Cannot register tab - service not initialized";
    return;
  }

  if (!web_contents) {
    return;
  }

  // Check if already registered
  if (tab_trackers_.find(web_contents) != tab_trackers_.end()) {
    return;
  }

  LOG(INFO) << "🔵 DataSipper: Registering tab for URL: "
            << web_contents->GetLastCommittedURL().spec();

  // Create tracker
  auto tracker = std::make_unique<TabTracker>();
  tracker->web_contents = web_contents;

  // Create PageDataDetector for this tab
  tracker->detector = std::make_unique<PageDataDetector>(web_contents);

  // Enable live tracking with callback
  tracker->detector->EnableLiveTracking(
      base::BindRepeating(&DataSipperService::OnDataExtracted,
                         base::Unretained(this),
                         web_contents));

  // Enable stream tracking with callback
  tracker->detector->EnableStreamTracking(
      base::BindRepeating(&DataSipperService::OnStreamDetected,
                         base::Unretained(this),
                         web_contents));

  // Store tracker
  tab_trackers_[web_contents] = std::move(tracker);

  LOG(INFO) << "✅ DataSipper: Tab registered with live extraction and stream tracking";
}

void DataSipperService::UnregisterTab(content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  auto it = tab_trackers_.find(web_contents);
  if (it == tab_trackers_.end()) {
    return;
  }

  LOG(INFO) << "🔵 DataSipper: Unregistering tab for URL: "
            << web_contents->GetLastCommittedURL().spec();

  // Disable live tracking and stream tracking
  if (it->second->detector) {
    it->second->detector->DisableLiveTracking();
    it->second->detector->DisableStreamTracking();
  }

  // Remove tracker
  tab_trackers_.erase(it);

  LOG(INFO) << "✅ DataSipper: Tab unregistered";
}

void DataSipperService::OnDataExtracted(
    content::WebContents* web_contents,
    const std::vector<ExtractedDataSource>& sources) {

  if (!web_contents) {
    return;
  }

  const GURL& url = web_contents->GetLastCommittedURL();

  LOG(INFO) << "🔍 DataSipper: Extracted " << sources.size()
            << " data sources from " << url.spec();

  // Log extracted sources
  for (const auto& source : sources) {
    LOG(INFO) << "  - " << DataSourceTypeToString(source.type)
              << " (confidence: " << source.confidence_score << ")";
  }

  // Trigger matching workflows
  TriggerMatchingWorkflows(web_contents, url, sources);
}

void DataSipperService::OnStreamDetected(
    content::WebContents* web_contents,
    const StreamDefinition& stream) {
  // web_contents may be null for global network streams
  GURL url;
  if (web_contents) {
    url = web_contents->GetLastCommittedURL();
  }

  // Convert stream type to string for logging
  const char* type_str = "Unknown";
  switch (stream.type) {
    case StreamType::DOM_MUTATION:
      type_str = "DOM Mutation";
      break;
    case StreamType::NETWORK_PATTERN:
      type_str = "Network Pattern";
      break;
    case StreamType::WEBSOCKET_STREAM:
      type_str = "WebSocket Stream";
      break;
  }

  if (web_contents) {
    LOG(INFO) << "📊 DataSipper: Stream detected on " << url.spec();
  } else {
    LOG(INFO) << "📊 DataSipper: Stream detected (global/network)";
  }
  LOG(INFO) << "  - ID: " << stream.stream_id;
  LOG(INFO) << "  - Label: " << stream.label;
  LOG(INFO) << "  - Type: " << type_str;
  LOG(INFO) << "  - Pattern: " << stream.source_pattern;
  LOG(INFO) << "  - Avg Interval: " << stream.timing.avg_interval_ms << "ms";
  LOG(INFO) << "  - Event Count: " << stream.event_count;
  LOG(INFO) << "  - Is Regular: " << (stream.timing.is_regular ? "yes" : "no");

  // Register stream in registry
  if (stream_registry_) {
    std::string stream_id = stream_registry_->RegisterStream(stream);
    LOG(INFO) << "📊 StreamRegistry: Stream registered as " << stream_id;
  }

  // TODO: Store stream definition in database
  // TODO: Check for workflows that should subscribe to this stream
  // TODO: Broadcast stream info to WebUI for user visibility
}

void DataSipperService::TriggerMatchingWorkflows(
    content::WebContents* web_contents,
    const GURL& url,
    const std::vector<ExtractedDataSource>& sources) {

  if (!workflow_storage_ || !workflow_engine_ || !web_contents) {
    return;
  }

  // Get workflows matching this URL
  std::vector<Workflow> matching_workflows =
      workflow_storage_->GetWorkflowsForUrl(url.spec());

  if (matching_workflows.empty()) {
    LOG(INFO) << "No workflows match URL: " << url.spec();
    return;
  }

  LOG(INFO) << "🎯 DataSipper: Found " << matching_workflows.size()
            << " matching workflow(s) for " << url.spec();

  // Convert extracted sources to base::Value format
  std::vector<base::Value> data_sources;
  for (const auto& source : sources) {
    base::Value::Dict source_dict;
    source_dict.Set("type", static_cast<int>(source.type));
    source_dict.Set("confidence_score", source.confidence_score);
    source_dict.Set("label", source.label);
    source_dict.Set("selector_path", source.selector_path);

    // Include the parsed data directly (it's already a base::Value)
    source_dict.Set("data", source.parsed_data.Clone());

    data_sources.push_back(base::Value(std::move(source_dict)));
  }

  // Get URL loader factory from browser context
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      web_contents->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  // Update workflow engine's URL loader factory
  // Note: This is a temporary solution. Ideally, we'd set it once at init
  // but we need a BrowserContext which we don't have until a tab is created
  if (workflow_engine_) {
    // We can't directly update it, so we'll need to recreate the engine
    // Or better: make the engine accept it per-workflow execution
    // For now, let's just recreate it with the factory
    workflow_engine_ = std::make_unique<WorkflowEngine>(
        workflow_storage_.get(), url_loader_factory);
  }

  // Execute each matching workflow
  for (const auto& workflow : matching_workflows) {
    if (!workflow.enabled) {
      LOG(INFO) << "  - Skipping disabled workflow: " << workflow.name;
      continue;
    }

    LOG(INFO) << "  - Executing workflow: " << workflow.name;

    // Execute workflow with extracted data sources
    workflow_engine_->ExecuteWorkflow(
        workflow,
        web_contents,
        base::BindOnce([](const std::string& workflow_name,
                          const WorkflowExecutionContext& context) {
          if (context.status == WorkflowExecutionContext::Status::COMPLETED) {
            LOG(INFO) << "✅ Workflow '" << workflow_name
                      << "' completed successfully";
          } else if (context.status == WorkflowExecutionContext::Status::FAILED) {
            LOG(ERROR) << "❌ Workflow '" << workflow_name
                       << "' failed: " << context.error_message;
          }
        }, workflow.name),
        data_sources);
  }
}

void DataSipperService::OnNetworkRequestCaptured(
    side_panel::mojom::NetworkRequestDataPtr request) {
  if (!request) {
    return;
  }

  DVLOG(1) << "📡 DataSipperService: Processing network request: "
           << request->method << " " << request->url;

  // Process network event for stream detection
  if (network_stream_analyzer_ && network_stream_analyzer_->IsAnalyzing()) {
    NetworkEvent event = ConvertToNetworkEvent(request);
    network_stream_analyzer_->ProcessNetworkEvent(event);
  }

  // TODO: Extract data from network requests (JSON-LD, structured data, etc.)
  // TODO: Match against registered workflows and trigger actions

  // For now, this method is called by the bridge for future extraction/workflow
  // processing. Broadcasting to WebUI is handled by the bridge directly.
}

}  // namespace datasipper
