// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_DATASIPPER_DATASIPPER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_DATASIPPER_DATASIPPER_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class DataSipperUI;

namespace content {
class WebContents;
}

namespace datasipper {
class DataSipperService;
}

class DataSipperPageHandler : public side_panel::mojom::DataSipperPageHandler {
 public:
  DataSipperPageHandler(
      mojo::PendingReceiver<side_panel::mojom::DataSipperPageHandler> receiver,
      DataSipperUI* datasipper_ui,
      content::WebContents* web_contents);
  ~DataSipperPageHandler() override;

  DataSipperPageHandler(const DataSipperPageHandler&) = delete;
  DataSipperPageHandler& operator=(const DataSipperPageHandler&) = delete;

  // side_panel::mojom::DataSipperPageHandler:
  void GetExtractedData(GetExtractedDataCallback callback) override;
  void GetWorkflows(GetWorkflowsCallback callback) override;
  void CreateWorkflow(const std::string& name,
                      const std::string& url_pattern,
                      CreateWorkflowCallback callback) override;
  void DeleteWorkflow(const std::string& workflow_id,
                      DeleteWorkflowCallback callback) override;
  void ToggleWorkflow(const std::string& workflow_id,
                      bool enabled,
                      ToggleWorkflowCallback callback) override;
  void ShowUI() override;
  void AddObserver(mojo::PendingRemote<side_panel::mojom::DataSipperObserver>
                       observer) override;

  // Stream management methods
  void GetDetectedStreams(GetDetectedStreamsCallback callback) override;
  void GetStreamDetails(const std::string& stream_id,
                        GetStreamDetailsCallback callback) override;
  void GetStreamEvents(const std::string& stream_id,
                       int32_t limit,
                       GetStreamEventsCallback callback) override;
  void SubscribeToStream(const std::string& stream_id,
                         SubscribeToStreamCallback callback) override;
  void UnsubscribeFromStream(const std::string& subscription_id,
                             UnsubscribeFromStreamCallback callback) override;
  void SetStreamActive(const std::string& stream_id,
                       bool active,
                       SetStreamActiveCallback callback) override;
  void DeleteStream(const std::string& stream_id,
                    DeleteStreamCallback callback) override;
  void ExportStreamAsWorkflow(const std::string& stream_id,
                              ExportStreamAsWorkflowCallback callback) override;

  // Called by DataSipperService to forward real network data to JavaScript
  void SendNetworkRequest(side_panel::mojom::NetworkRequestDataPtr request);

  // Check if this handler has an observer bound
  bool HasObserver() const { return observer_.is_bound(); }

 private:
  datasipper::DataSipperService* GetDataSipperService();

  mojo::Receiver<side_panel::mojom::DataSipperPageHandler> receiver_;
  mojo::Remote<side_panel::mojom::DataSipperObserver> observer_;
  raw_ptr<DataSipperUI> datasipper_ui_ = nullptr;
  raw_ptr<content::WebContents> web_contents_;

  base::WeakPtrFactory<DataSipperPageHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_DATASIPPER_DATASIPPER_PAGE_HANDLER_H_
