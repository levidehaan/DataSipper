// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_STORAGE_WEBSOCKET_MESSAGE_STORAGE_H_
#define COMPONENTS_DATASIPPER_STORAGE_WEBSOCKET_MESSAGE_STORAGE_H_

#include "base/memory/weak_ptr.h"

namespace datasipper {

class DataSipperDatabase;

// Storage manager for WebSocket messages
class WebSocketMessageStorage {
 public:
  explicit WebSocketMessageStorage(DataSipperDatabase* database);
  ~WebSocketMessageStorage();

  WebSocketMessageStorage(const WebSocketMessageStorage&) = delete;
  WebSocketMessageStorage& operator=(const WebSocketMessageStorage&) = delete;

 private:
  DataSipperDatabase* database_;
  base::WeakPtrFactory<WebSocketMessageStorage> weak_factory_{this};
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_STORAGE_WEBSOCKET_MESSAGE_STORAGE_H_
