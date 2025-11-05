// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DATASIPPER_DATASIPPER_NETWORK_SERVICE_OBSERVER_H_
#define CONTENT_BROWSER_DATASIPPER_DATASIPPER_NETWORK_SERVICE_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {
class BrowserContext;
}

namespace datasipper {
class DataSipperService;
struct NetworkEvent;
}

namespace content {

// Bridge between network service and DataSipper service in browser process
class DataSipperNetworkServiceObserver
    : public network::mojom::NetworkServiceObserver {
 public:
  explicit DataSipperNetworkServiceObserver(BrowserContext* browser_context);
  ~DataSipperNetworkServiceObserver() override;

  DataSipperNetworkServiceObserver(const DataSipperNetworkServiceObserver&) = delete;
  DataSipperNetworkServiceObserver& operator=(const DataSipperNetworkServiceObserver&) = delete;

  // network::mojom::NetworkServiceObserver implementation:
  void OnDataSipperNetworkEvent(
      const std::string& event_data) override;

  // Setup and teardown
  void Initialize();
  void Shutdown();

 private:
  void OnNetworkEventReceived(std::unique_ptr<datasipper::NetworkEvent> event);
  datasipper::DataSipperService* GetDataSipperService();

  raw_ptr<BrowserContext> browser_context_;
  mojo::Receiver<network::mojom::NetworkServiceObserver> receiver_{this};

  base::WeakPtrFactory<DataSipperNetworkServiceObserver> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DATASIPPER_DATASIPPER_NETWORK_SERVICE_OBSERVER_H_
