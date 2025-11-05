// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/datasipper_service.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
// Note: Cannot include chrome/browser headers here due to circular dependencies
// DataSipperNetworkBridge (in chrome/browser layer) will handle broadcasting
#include "chrome/common/chrome_features.h"
#include "components/datasipper/datasipper_database.h"
#include "components/datasipper/extraction/extraction_result.h"
#include "components/datasipper/extraction/page_data_detector.h"
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

  // Shutdown workflow system
  workflow_engine_.reset();
  workflow_storage_.reset();

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
  // TODO: Initialize storage components when we have the header files
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

  // Store tracker
  tab_trackers_[web_contents] = std::move(tracker);

  LOG(INFO) << "✅ DataSipper: Tab registered with live extraction tracking";
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

  // Disable live tracking
  if (it->second->detector) {
    it->second->detector->DisableLiveTracking();
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

  // TODO: Extract data from network requests (JSON-LD, structured data, etc.)
  // TODO: Match against registered workflows and trigger actions

  // For now, this method is called by the bridge for future extraction/workflow
  // processing. Broadcasting to WebUI is handled by the bridge directly.
}

}  // namespace datasipper
