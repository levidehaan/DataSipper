// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/datasipper/datasipper_network_client.h"

#include "base/logging.h"
#include "chrome/browser/datasipper/datasipper_network_bridge.h"
#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper.mojom.h"

DataSipperNetworkClient::DataSipperNetworkClient() {
  DVLOG(1) << "✅ DataSipperNetworkClient created";
}

DataSipperNetworkClient::~DataSipperNetworkClient() {
  DVLOG(1) << "🔵 DataSipperNetworkClient destroyed";
}

mojo::PendingRemote<datasipper::mojom::DataSipperNetworkClient>
DataSipperNetworkClient::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void DataSipperNetworkClient::OnNetworkRequestCaptured(
    datasipper::mojom::NetworkRequestDataPtr request) {
  if (!request) {
    return;
  }

  DVLOG(1) << "📡 DataSipperNetworkClient: Received network request from "
              "network service: "
           << request->method << " " << request->url;

  // Convert from datasipper::mojom::NetworkRequestData to
  // side_panel::mojom::NetworkRequestData
  auto ui_request = side_panel::mojom::NetworkRequestData::New();
  ui_request->request_id = request->request_id;
  ui_request->url = request->url;
  ui_request->method = request->method;
  ui_request->type = request->type;
  ui_request->timestamp_ms = request->timestamp_ms;
  ui_request->status_code = request->status_code;
  ui_request->status_text = request->status_text;
  ui_request->request_headers = std::move(request->request_headers);
  ui_request->response_headers = std::move(request->response_headers);
  ui_request->request_body = request->request_body;
  ui_request->response_body = request->response_body;
  ui_request->size_bytes = request->size_bytes;
  ui_request->duration_ms = request->duration_ms;

  // Forward to DataSipperNetworkBridge which distributes to the service
  DataSipperNetworkBridge::GetInstance()->OnNetworkRequestCaptured(
      std::move(ui_request));
}
