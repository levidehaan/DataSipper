// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/datasipper/datasipper_network_service_observer.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "chrome/browser/datasipper/datasipper_service.h"
#include "chrome/browser/datasipper/datasipper_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/datasipper/common/network_event.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace content {

DataSipperNetworkServiceObserver::DataSipperNetworkServiceObserver(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context_);
}

DataSipperNetworkServiceObserver::~DataSipperNetworkServiceObserver() = default;

void DataSipperNetworkServiceObserver::Initialize() {
  // Connect to network service as observer
  auto* storage_partition = browser_context_->GetDefaultStoragePartition();
  if (storage_partition && storage_partition->GetNetworkContext()) {
    storage_partition->GetNetworkContext()->AddNetworkServiceObserver(
        receiver_.BindNewPipeAndPassRemote());
  }

  LOG(INFO) << "DataSipper network service observer initialized";
}

void DataSipperNetworkServiceObserver::Shutdown() {
  receiver_.reset();
  LOG(INFO) << "DataSipper network service observer shutdown";
}

void DataSipperNetworkServiceObserver::OnDataSipperNetworkEvent(
    const std::string& event_data) {
  // Parse JSON event data
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(event_data);
  if (!parsed_json.has_value() || !parsed_json->is_dict()) {
    LOG(ERROR) << "Failed to parse DataSipper network event JSON";
    return;
  }

  // Convert to NetworkEvent
  auto event = datasipper::NetworkEvent::FromDict(parsed_json->GetDict());
  if (!event) {
    LOG(ERROR) << "Failed to create NetworkEvent from JSON data";
    return;
  }

  OnNetworkEventReceived(std::move(event));
}

void DataSipperNetworkServiceObserver::OnNetworkEventReceived(
    std::unique_ptr<datasipper::NetworkEvent> event) {
  auto* service = GetDataSipperService();
  if (service) {
    service->OnNetworkEvent(std::move(event));
  } else {
    DVLOG(1) << "DataSipper service not available, dropping network event";
  }
}

datasipper::DataSipperService*
DataSipperNetworkServiceObserver::GetDataSipperService() {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  return profile ? DataSipperServiceFactory::GetForProfile(profile) : nullptr;
}

}  // namespace content
