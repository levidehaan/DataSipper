// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/datasipper_network_observer.h"

#include <sstream>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

// RequestInfo implementation
DataSipperNetworkObserver::RequestInfo::RequestInfo() = default;
DataSipperNetworkObserver::RequestInfo::~RequestInfo() = default;

DataSipperNetworkObserver::DataSipperNetworkObserver(
    mojo::SharedRemote<datasipper::mojom::DataSipperNetworkClient> client)
    : client_(std::move(client)) {
  if (client_) {
    DVLOG(1) << "✅ DataSipper network observer initialized with shared Mojo client";
  } else {
    DVLOG(1) << "⚠️ DataSipper network observer initialized WITHOUT Mojo client";
  }
}

DataSipperNetworkObserver::~DataSipperNetworkObserver() {
  DVLOG(1) << "DataSipper network observer destroyed";
}

void DataSipperNetworkObserver::OnRequestStarted(const net::URLRequest* request) {
  if (!request) return;

  auto info = std::make_unique<RequestInfo>();
  info->url = request->url().spec();
  info->method = request->method();
  info->start_time = base::Time::Now();
  info->priority = request->priority();

  CaptureRequestHeaders(request, info.get());
  CaptureRequestBody(request, info.get());

  active_requests_[request] = std::move(info);

  DVLOG(2) << "DataSipper: Request started - " << request->method()
           << " " << request->url().spec();
}

void DataSipperNetworkObserver::OnRequestPriorityChanged(
    const net::URLRequest* request,
    net::RequestPriority priority) {
  auto it = active_requests_.find(request);
  if (it != active_requests_.end()) {
    it->second->priority = priority;
    DVLOG(3) << "DataSipper: Priority changed for " << request->url().spec();
  }
}

void DataSipperNetworkObserver::OnResponseStarted(
    const net::URLRequest* request,
    const mojom::URLResponseHead* response_head) {
  auto it = active_requests_.find(request);
  if (it == active_requests_.end()) {
    // If we didn't catch the request start, create an entry now
    OnRequestStarted(request);
    it = active_requests_.find(request);
  }

  if (it != active_requests_.end() && response_head) {
    RequestInfo* info = it->second.get();
    info->response_start_time = base::Time::Now();
    info->response_code = response_head->headers ?
        response_head->headers->response_code() : 0;

    CaptureResponseHeaders(response_head, info);

    DVLOG(2) << "DataSipper: Response started - " << info->response_code
             << " for " << request->url().spec();
  }
}

void DataSipperNetworkObserver::OnBeforeRead(const net::URLRequest* request,
                                           int buffer_size) {
  DVLOG(3) << "DataSipper: Before read " << buffer_size << " bytes for "
           << request->url().spec();
}

void DataSipperNetworkObserver::OnDataRead(const net::URLRequest* request,
                                         const char* data,
                                         int bytes_read) {
  auto it = active_requests_.find(request);
  if (it != active_requests_.end() && capture_response_body_ && data && bytes_read > 0) {
    RequestInfo* info = it->second.get();

    // Limit response body size to prevent memory issues
    if (info->response_body.size() + bytes_read <= max_body_size_) {
      info->response_body.append(data, bytes_read);
    }

    info->total_bytes_read += bytes_read;

    DVLOG(3) << "DataSipper: Read " << bytes_read << " bytes for "
             << request->url().spec();
  }
}

void DataSipperNetworkObserver::OnReadCompleted(const net::URLRequest* request,
                                              int bytes_read) {
  DVLOG(3) << "DataSipper: Read completed " << bytes_read << " bytes for "
           << request->url().spec();
}

void DataSipperNetworkObserver::OnResponseCompleted(const net::URLRequest* request,
                                                  int error_code) {
  auto it = active_requests_.find(request);
  if (it != active_requests_.end()) {
    RequestInfo* info = it->second.get();
    info->completion_time = base::Time::Now();
    info->error_code = error_code;

    DVLOG(2) << "DataSipper: Request completed - " << error_code
             << " for " << request->url().spec();

    // Process the completed request
    ProcessCompletedRequest(std::move(it->second));
    active_requests_.erase(it);
  }
}

void DataSipperNetworkObserver::CaptureRequestHeaders(const net::URLRequest* request,
                                                    RequestInfo* info) {
  if (!request || !info) return;

  const net::HttpRequestHeaders& headers = request->extra_request_headers();
  std::string headers_string;

  net::HttpRequestHeaders::Iterator it(headers);
  while (it.GetNext()) {
    headers_string += it.name() + ": " + it.value() + "\n";
  }

  info->request_headers = headers_string;
}

void DataSipperNetworkObserver::CaptureRequestBody(const net::URLRequest* request,
                                                 RequestInfo* info) {
  if (!request || !info || !capture_request_body_) return;

  // TODO: Implement request body capture
  // The URLRequest API has changed - we need to capture body differently
  // This may require hooking into URLLoader at a different point
  info->request_body = "[Request body capture not yet implemented]";
}

void DataSipperNetworkObserver::CaptureResponseHeaders(
    const mojom::URLResponseHead* response_head,
    RequestInfo* info) {
  if (!response_head || !response_head->headers || !info) return;

  std::string headers_string;
  size_t iter = 0;
  std::string name, value;

  while (response_head->headers->EnumerateHeaderLines(&iter, &name, &value)) {
    headers_string += name + ": " + value + "\n";
  }

  info->response_headers = headers_string;
}

void DataSipperNetworkObserver::ProcessCompletedRequest(
    std::unique_ptr<RequestInfo> info) {
  if (!info) return;

  // Send to DataSipper panel for real-time display
  SendToDataSipperPanel(*info);

  // Store in database for persistence
  StoreInDatabase(*info);
}

void DataSipperNetworkObserver::SendToDataSipperPanel(const RequestInfo& info) {
  // Check if we have a Mojo client to send to
  if (!client_.is_bound()) {
    DVLOG(2) << "DataSipper: No Mojo client bound, skipping network data transmission";
    return;
  }

  // Convert RequestInfo to Mojo NetworkRequestData
  auto request = datasipper::mojom::NetworkRequestData::New();

  // Generate request ID from start time
  request->request_id = base::NumberToString(
      info.start_time.InMillisecondsSinceUnixEpoch());
  request->url = info.url;
  request->method = info.method;
  request->type = "fetch";  // Default type
  request->timestamp_ms = info.start_time.InMillisecondsSinceUnixEpoch();
  request->status_code = info.response_code;
  request->status_text = info.response_code == 200 ? "OK" : "Error";

  // Parse headers from strings to maps
  // Request headers
  std::istringstream req_stream(info.request_headers);
  std::string line;
  while (std::getline(req_stream, line)) {
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
      std::string name = line.substr(0, colon_pos);
      std::string value = line.substr(colon_pos + 1);
      // Trim whitespace
      name = std::string(base::TrimWhitespaceASCII(name, base::TRIM_ALL));
      value = std::string(base::TrimWhitespaceASCII(value, base::TRIM_ALL));
      request->request_headers[name] = value;
    }
  }

  // Response headers
  std::istringstream resp_stream(info.response_headers);
  while (std::getline(resp_stream, line)) {
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
      std::string name = line.substr(0, colon_pos);
      std::string value = line.substr(colon_pos + 1);
      // Trim whitespace
      name = std::string(base::TrimWhitespaceASCII(name, base::TRIM_ALL));
      value = std::string(base::TrimWhitespaceASCII(value, base::TRIM_ALL));
      request->response_headers[name] = value;
    }
  }

  request->request_body = info.request_body;
  request->response_body = info.response_body;
  request->size_bytes = info.total_bytes_read;

  // Calculate duration in milliseconds
  if (!info.completion_time.is_null() && !info.start_time.is_null()) {
    request->duration_ms =
        (info.completion_time - info.start_time).InMilliseconds();
  } else {
    request->duration_ms = 0;
  }

  DVLOG(1) << "🌐 DataSipper: Sending via Mojo to browser - " << info.method
           << " " << info.url << " (" << info.response_code << ")";

  // Send through Mojo to browser process
  client_->OnNetworkRequestCaptured(std::move(request));
}

void DataSipperNetworkObserver::StoreInDatabase(const RequestInfo& info) {
  // TODO: Implement database storage
  // This will involve storing the request/response data in SQLite
  // for historical viewing and analysis

  DVLOG(1) << "DataSipper: Storing in DB - " << info.method
           << " " << info.url << " (" << info.response_code << ")";
}

}  // namespace network
