// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DATASIPPER_NETWORK_OBSERVER_H_
#define SERVICES_NETWORK_DATASIPPER_NETWORK_OBSERVER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/datasipper/mojom/network_observer.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/base/request_priority.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace net {
class URLRequest;
}

namespace network {

// Observer class that captures and processes network requests and responses
// for the DataSipper monitoring panel.
class DataSipperNetworkObserver {
 public:
  // Constructor accepts optional Mojo shared client for cross-process communication
  explicit DataSipperNetworkObserver(
      mojo::SharedRemote<datasipper::mojom::DataSipperNetworkClient> client =
          mojo::SharedRemote<datasipper::mojom::DataSipperNetworkClient>());
  ~DataSipperNetworkObserver();

  DataSipperNetworkObserver(const DataSipperNetworkObserver&) = delete;
  DataSipperNetworkObserver& operator=(const DataSipperNetworkObserver&) = delete;

  // Network event callbacks
  void OnRequestStarted(const net::URLRequest* request);
  void OnRequestPriorityChanged(const net::URLRequest* request,
                               net::RequestPriority priority);
  void OnResponseStarted(const net::URLRequest* request,
                        const mojom::URLResponseHead* response_head);
  void OnBeforeRead(const net::URLRequest* request, int buffer_size);
  void OnDataRead(const net::URLRequest* request,
                 const char* data,
                 int bytes_read);
  void OnReadCompleted(const net::URLRequest* request, int bytes_read);
  void OnResponseCompleted(const net::URLRequest* request, int error_code);

 private:
  struct RequestInfo {
    RequestInfo();
    ~RequestInfo();

    std::string url;
    std::string method;
    base::Time start_time;
    net::RequestPriority priority;
    std::string request_headers;
    std::string request_body;

    // Response data
    int response_code = 0;
    std::string response_headers;
    std::string response_body;
    base::Time response_start_time;
    base::Time completion_time;
    int error_code = 0;
    int64_t total_bytes_read = 0;
  };

  // Helper methods
  void CaptureRequestHeaders(const net::URLRequest* request, RequestInfo* info);
  void CaptureRequestBody(const net::URLRequest* request, RequestInfo* info);
  void CaptureResponseHeaders(const mojom::URLResponseHead* response_head,
                             RequestInfo* info);
  void ProcessCompletedRequest(std::unique_ptr<RequestInfo> info);
  void SendToDataSipperPanel(const RequestInfo& info);
  void StoreInDatabase(const RequestInfo& info);

  // Request tracking
  std::map<const net::URLRequest*, std::unique_ptr<RequestInfo>> active_requests_;

  // Configuration
  bool capture_request_body_ = true;
  bool capture_response_body_ = true;
  size_t max_body_size_ = 1024 * 1024;  // 1MB max

  // Mojo shared client for sending network data to browser process
  mojo::SharedRemote<datasipper::mojom::DataSipperNetworkClient> client_;

  base::WeakPtrFactory<DataSipperNetworkObserver> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_DATASIPPER_NETWORK_OBSERVER_H_
