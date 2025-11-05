// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/datasipper/datasipper_network_bridge.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper_page_handler.h"
#include "components/datasipper/datasipper_service.h"

// static
DataSipperNetworkBridge* DataSipperNetworkBridge::GetInstance() {
  static base::NoDestructor<DataSipperNetworkBridge> instance;
  return instance.get();
}

DataSipperNetworkBridge::DataSipperNetworkBridge() = default;
DataSipperNetworkBridge::~DataSipperNetworkBridge() = default;

void DataSipperNetworkBridge::SetService(
    datasipper::DataSipperService* service) {
  service_ = service;
  if (service_) {
    LOG(INFO) << "✅ DataSipperNetworkBridge: Service connected";
  } else {
    LOG(INFO) << "❌ DataSipperNetworkBridge: Service disconnected";
  }
}

void DataSipperNetworkBridge::RegisterPageHandler(
    DataSipperPageHandler* handler) {
  if (!handler) {
    return;
  }

  // Check if already registered
  auto it = std::find(page_handlers_.begin(), page_handlers_.end(), handler);
  if (it != page_handlers_.end()) {
    return;
  }

  page_handlers_.push_back(handler);
  LOG(INFO) << "✅ DataSipperNetworkBridge: Registered page handler ("
            << page_handlers_.size() << " total handlers)";
}

void DataSipperNetworkBridge::UnregisterPageHandler(
    DataSipperPageHandler* handler) {
  if (!handler) {
    return;
  }

  auto it = std::find(page_handlers_.begin(), page_handlers_.end(), handler);
  if (it == page_handlers_.end()) {
    return;
  }

  page_handlers_.erase(it);
  LOG(INFO) << "🔵 DataSipperNetworkBridge: Unregistered page handler ("
            << page_handlers_.size() << " remaining handlers)";
}

void DataSipperNetworkBridge::OnNetworkRequestCaptured(
    side_panel::mojom::NetworkRequestDataPtr request) {
  if (!request) {
    return;
  }

  DVLOG(1) << "📡 DataSipperNetworkBridge: Captured " << request->method
           << " " << request->url;

  // Forward to service for extraction and workflow processing
  if (service_) {
    DVLOG(1) << "  → Forwarding to service for processing";
    service_->OnNetworkRequestCaptured(request.Clone());
  }

  // Broadcast to all registered page handlers for WebUI display
  if (!page_handlers_.empty()) {
    DVLOG(1) << "  → Broadcasting to " << page_handlers_.size()
             << " page handler(s)";

    for (auto* handler : page_handlers_) {
      if (handler) {
        // Clone the request for each handler
        auto request_clone = request.Clone();
        handler->SendNetworkRequest(std::move(request_clone));
      }
    }
  }
}
