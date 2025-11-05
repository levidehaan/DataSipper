// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATASIPPER_DATASIPPER_NETWORK_CLIENT_H_
#define CHROME_BROWSER_DATASIPPER_DATASIPPER_NETWORK_CLIENT_H_

#include "components/datasipper/mojom/network_observer.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

// Browser-side Mojo receiver that receives network requests from the network
// service process and forwards them to DataSipperNetworkBridge for distribution
// to the DataSipper UI.
class DataSipperNetworkClient
    : public datasipper::mojom::DataSipperNetworkClient {
 public:
  DataSipperNetworkClient();
  ~DataSipperNetworkClient() override;

  DataSipperNetworkClient(const DataSipperNetworkClient&) = delete;
  DataSipperNetworkClient& operator=(const DataSipperNetworkClient&) = delete;

  // Returns a PendingRemote for passing to the network service
  mojo::PendingRemote<datasipper::mojom::DataSipperNetworkClient>
  BindNewPipeAndPassRemote();

  // datasipper::mojom::DataSipperNetworkClient implementation:
  void OnNetworkRequestCaptured(
      datasipper::mojom::NetworkRequestDataPtr request) override;

 private:
  mojo::Receiver<datasipper::mojom::DataSipperNetworkClient> receiver_{this};
};

#endif  // CHROME_BROWSER_DATASIPPER_DATASIPPER_NETWORK_CLIENT_H_
