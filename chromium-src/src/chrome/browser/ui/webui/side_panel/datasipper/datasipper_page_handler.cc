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
