// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/storage/websocket_message_storage.h"

#include "components/datasipper/database/datasipper_database.h"

namespace datasipper {

WebSocketMessageStorage::WebSocketMessageStorage(DataSipperDatabase* database)
    : database_(database) {}

WebSocketMessageStorage::~WebSocketMessageStorage() = default;

}  // namespace datasipper
