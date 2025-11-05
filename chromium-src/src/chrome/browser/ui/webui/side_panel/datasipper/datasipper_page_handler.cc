// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper_page_handler.h"

#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/datasipper/datasipper_network_bridge.h"
#include "chrome/browser/datasipper/datasipper_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper_ui.h"
#include "components/datasipper/datasipper_service.h"
#include "content/public/browser/web_contents.h"

DataSipperPageHandler::DataSipperPageHandler(
    mojo::PendingReceiver<side_panel::mojom::DataSipperPageHandler> receiver,
    DataSipperUI* datasipper_ui,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      datasipper_ui_(datasipper_ui),
      web_contents_(web_contents) {
  // Register with DataSipperNetworkBridge to receive network events
  DataSipperNetworkBridge::GetInstance()->RegisterPageHandler(this);
}

DataSipperPageHandler::~DataSipperPageHandler() {
  // Unregister from DataSipperNetworkBridge
  DataSipperNetworkBridge::GetInstance()->UnregisterPageHandler(this);
}

datasipper::DataSipperService* DataSipperPageHandler::GetDataSipperService() {
  if (!web_contents_) {
    return nullptr;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (!profile) {
    return nullptr;
  }

  return DataSipperServiceFactory::GetForProfile(profile);
}

void DataSipperPageHandler::GetExtractedData(
    GetExtractedDataCallback callback) {
  // TODO: Get extracted data from PageDataDetector for current tab
  std::vector<side_panel::mojom::ExtractedDataPtr> data;

  // Placeholder
  auto item = side_panel::mojom::ExtractedData::New();
  item->type = "JSON-LD";
  item->label = "Product Data";
  item->data = "{\"name\": \"Example Product\"}";
  data.push_back(std::move(item));

  std::move(callback).Run(std::move(data));
}

void DataSipperPageHandler::GetWorkflows(GetWorkflowsCallback callback) {
  // TODO: Get workflows from DataSipperService
  std::vector<side_panel::mojom::WorkflowInfoPtr> workflows;

  // Placeholder
  auto workflow = side_panel::mojom::WorkflowInfo::New();
  workflow->id = "example-workflow";
  workflow->name = "Example Workflow";
  workflow->url_pattern = ".*amazon.com.*";
  workflow->enabled = true;
  workflows.push_back(std::move(workflow));

  std::move(callback).Run(std::move(workflows));
}

void DataSipperPageHandler::CreateWorkflow(const std::string& name,
                                           const std::string& url_pattern,
                                           CreateWorkflowCallback callback) {
  // TODO: Create workflow via DataSipperService
  std::move(callback).Run(true, "workflow-id-123");
}

void DataSipperPageHandler::DeleteWorkflow(const std::string& workflow_id,
                                           DeleteWorkflowCallback callback) {
  // TODO: Delete workflow via DataSipperService
  std::move(callback).Run(true);
}

void DataSipperPageHandler::ToggleWorkflow(const std::string& workflow_id,
                                           bool enabled,
                                           ToggleWorkflowCallback callback) {
  // TODO: Toggle workflow via DataSipperService
  std::move(callback).Run(true);
}

void DataSipperPageHandler::ShowUI() {
  auto embedder = datasipper_ui_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}

void DataSipperPageHandler::AddObserver(
    mojo::PendingRemote<side_panel::mojom::DataSipperObserver> observer) {
  observer_.reset();
  observer_.Bind(std::move(observer));
}

void DataSipperPageHandler::GetDetectedStreams(
    GetDetectedStreamsCallback callback) {
  datasipper::DataSipperService* service = GetDataSipperService();
  std::vector<side_panel::mojom::StreamInfoPtr> streams;

  if (service && service->stream_registry()) {
    // Get all streams from the registry
    const auto& all_streams = service->stream_registry()->GetAllStreams();

    for (const auto& [stream_id, stream_def] : all_streams) {
      auto stream_info = side_panel::mojom::StreamInfo::New();
      stream_info->stream_id = stream_def.stream_id;
      stream_info->label = stream_def.label;
      stream_info->type =
          static_cast<side_panel::mojom::StreamType>(stream_def.type);
      stream_info->source_pattern = stream_def.source_pattern;
      stream_info->url = stream_def.url;
      stream_info->first_seen =
          stream_def.first_seen.InMillisecondsSinceUnixEpoch();
      stream_info->last_seen =
          stream_def.last_seen.InMillisecondsSinceUnixEpoch();

      // Timing info
      auto timing = side_panel::mojom::StreamTimingInfo::New();
      timing->avg_interval_ms = stream_def.timing.avg_interval_ms;
      timing->std_deviation = stream_def.timing.std_deviation;
      timing->is_regular = stream_def.timing.is_regular;
      stream_info->timing = std::move(timing);

      stream_info->is_active = stream_def.is_active;
      stream_info->event_count = stream_def.event_count;
      stream_info->data_type =
          static_cast<side_panel::mojom::DataType>(stream_def.data_type);

      streams.push_back(std::move(stream_info));
    }
  }

  std::move(callback).Run(std::move(streams));
}

void DataSipperPageHandler::GetStreamDetails(
    const std::string& stream_id,
    GetStreamDetailsCallback callback) {
  datasipper::DataSipperService* service = GetDataSipperService();
  side_panel::mojom::StreamInfoPtr stream_info;

  if (service && service->stream_registry()) {
    auto stream_opt = service->stream_registry()->GetStream(stream_id);
    if (stream_opt.has_value()) {
      const auto& stream_def = stream_opt.value();
      stream_info = side_panel::mojom::StreamInfo::New();
      stream_info->stream_id = stream_def.stream_id;
      stream_info->label = stream_def.label;
      stream_info->type =
          static_cast<side_panel::mojom::StreamType>(stream_def.type);
      stream_info->source_pattern = stream_def.source_pattern;
      stream_info->url = stream_def.url;
      stream_info->first_seen =
          stream_def.first_seen.InMillisecondsSinceUnixEpoch();
      stream_info->last_seen =
          stream_def.last_seen.InMillisecondsSinceUnixEpoch();

      auto timing = side_panel::mojom::StreamTimingInfo::New();
      timing->avg_interval_ms = stream_def.timing.avg_interval_ms;
      timing->std_deviation = stream_def.timing.std_deviation;
      timing->is_regular = stream_def.timing.is_regular;
      stream_info->timing = std::move(timing);

      stream_info->is_active = stream_def.is_active;
      stream_info->event_count = stream_def.event_count;
      stream_info->data_type =
          static_cast<side_panel::mojom::DataType>(stream_def.data_type);
    }
  }

  std::move(callback).Run(std::move(stream_info));
}

void DataSipperPageHandler::GetStreamEvents(const std::string& stream_id,
                                             int32_t limit,
                                             GetStreamEventsCallback callback) {
  datasipper::DataSipperService* service = GetDataSipperService();
  std::vector<side_panel::mojom::StreamEventInfoPtr> events;

  if (service && service->stream_db_sync()) {
    // Get events from database
    auto db_events = service->stream_db_sync()->GetStreamEvents(stream_id, limit);

    for (const auto& event : db_events) {
      auto event_info = side_panel::mojom::StreamEventInfo::New();
      event_info->stream_id = event.stream_id;
      event_info->timestamp = event.timestamp.InMillisecondsSinceUnixEpoch();
      event_info->event_type = event.event_type;
      event_info->data = event.data;
      events.push_back(std::move(event_info));
    }
  }

  std::move(callback).Run(std::move(events));
}

void DataSipperPageHandler::SubscribeToStream(
    const std::string& stream_id,
    SubscribeToStreamCallback callback) {
  // TODO: Implement subscription management
  // For now, just return a simple subscription
  auto subscription = side_panel::mojom::StreamSubscription::New();
  subscription->subscription_id = stream_id + "_sub";
  subscription->stream_id = stream_id;

  std::move(callback).Run(std::move(subscription));
}

void DataSipperPageHandler::UnsubscribeFromStream(
    const std::string& subscription_id,
    UnsubscribeFromStreamCallback callback) {
  // TODO: Implement subscription cleanup
  std::move(callback).Run(true);
}

void DataSipperPageHandler::SetStreamActive(const std::string& stream_id,
                                             bool active,
                                             SetStreamActiveCallback callback) {
  datasipper::DataSipperService* service = GetDataSipperService();
  bool success = false;

  if (service && service->stream_registry()) {
    success = service->stream_registry()->SetStreamActive(stream_id, active);
  }

  std::move(callback).Run(success);
}

void DataSipperPageHandler::DeleteStream(const std::string& stream_id,
                                          DeleteStreamCallback callback) {
  datasipper::DataSipperService* service = GetDataSipperService();
  bool success = false;

  if (service) {
    // Delete from registry
    if (service->stream_registry()) {
      service->stream_registry()->RemoveStream(stream_id);
    }

    // Delete from database
    if (service->stream_db_sync()) {
      success = service->stream_db_sync()->DeleteStream(stream_id);
    }
  }

  std::move(callback).Run(success);
}

void DataSipperPageHandler::ExportStreamAsWorkflow(
    const std::string& stream_id,
    ExportStreamAsWorkflowCallback callback) {
  datasipper::DataSipperService* service = GetDataSipperService();
  side_panel::mojom::WorkflowInfoPtr workflow;

  if (service && service->stream_registry()) {
    auto stream_opt = service->stream_registry()->GetStream(stream_id);
    if (stream_opt.has_value()) {
      const auto& stream = stream_opt.value();

      // Create a workflow from the stream
      workflow = side_panel::mojom::WorkflowInfo::New();
      workflow->id = "workflow_from_" + stream_id;
      workflow->name = stream.label + " (exported)";
      workflow->url_pattern = stream.url;
      workflow->enabled = false;  // Start disabled

      // TODO: Convert stream detection logic to workflow configuration
    }
  }

  std::move(callback).Run(std::move(workflow));
}

void DataSipperPageHandler::SendNetworkRequest(
    side_panel::mojom::NetworkRequestDataPtr request) {
  if (!observer_.is_bound()) {
    DVLOG(2) << "DataSipperPageHandler: No observer bound, dropping request";
    return;
  }

  if (!request) {
    return;
  }

  DVLOG(1) << "📨 DataSipperPageHandler: Forwarding real network request to UI: "
           << request->method << " " << request->url;

  // Forward real network data to JavaScript observer
  observer_->OnNetworkRequestCaptured(std::move(request));
}
