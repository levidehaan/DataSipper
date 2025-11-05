// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATASIPPER_DATASIPPER_NETWORK_BRIDGE_H_
#define CHROME_BROWSER_DATASIPPER_DATASIPPER_NETWORK_BRIDGE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper.mojom.h"

namespace datasipper {
class DataSipperService;
}

class DataSipperPageHandler;

// Singleton bridge that connects the network service's DataSipperNetworkObserver
// to the browser process's DataSipperService. This avoids complex cross-process
// Mojo setup by providing a simple in-process bridge.
// Also handles broadcasting network requests to registered page handlers (WebUI).
class DataSipperNetworkBridge {
 public:
  static DataSipperNetworkBridge* GetInstance();

  // Called by DataSipperService when it's ready to receive network events
  void SetService(datasipper::DataSipperService* service);

  // Page handler registration for network data broadcasting
  void RegisterPageHandler(DataSipperPageHandler* handler);
  void UnregisterPageHandler(DataSipperPageHandler* handler);

  // Called by DataSipperNetworkObserver when a network request is captured
  void OnNetworkRequestCaptured(
      side_panel::mojom::NetworkRequestDataPtr request);

 private:
  friend class base::NoDestructor<DataSipperNetworkBridge>;

  DataSipperNetworkBridge();
  ~DataSipperNetworkBridge();

  raw_ptr<datasipper::DataSipperService> service_ = nullptr;

  // Registered page handlers that receive network data broadcasts
  std::vector<DataSipperPageHandler*> page_handlers_;
};

#endif  // CHROME_BROWSER_DATASIPPER_DATASIPPER_NETWORK_BRIDGE_H_
