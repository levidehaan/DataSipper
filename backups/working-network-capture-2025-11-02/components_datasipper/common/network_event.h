// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_COMMON_NETWORK_EVENT_H_
#define COMPONENTS_DATASIPPER_COMMON_NETWORK_EVENT_H_

#include <string>
#include "base/time/time.h"
#include "url/gurl.h"

namespace datasipper {

enum class EventType {
  HTTP_REQUEST,
  HTTP_RESPONSE,
  WEBSOCKET_CONNECT,
  WEBSOCKET_MESSAGE_SENT,
  WEBSOCKET_MESSAGE_RECEIVED,
  WEBSOCKET_DISCONNECT,
};

// Network event data structure for cross-process communication
struct NetworkEvent {
  std::string id;
  EventType type;
  base::Time timestamp;
  std::string connection_id;
  GURL url;
  std::string method;
  std::string headers;
  std::string body;
  int status_code = 0;
  std::string stream_name;
  std::string group_name;

  NetworkEvent();
  ~NetworkEvent();

  NetworkEvent(const NetworkEvent&);
  NetworkEvent& operator=(const NetworkEvent&);

  NetworkEvent(NetworkEvent&& other);
  NetworkEvent& operator=(NetworkEvent&& other);
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_COMMON_NETWORK_EVENT_H_
