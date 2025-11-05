// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_STORAGE_NETWORK_EVENT_STORAGE_H_
#define COMPONENTS_DATASIPPER_STORAGE_NETWORK_EVENT_STORAGE_H_

#include "base/memory/weak_ptr.h"

namespace datasipper {

class DataSipperDatabase;

// Storage manager for HTTP/HTTPS network events
class NetworkEventStorage {
 public:
  explicit NetworkEventStorage(DataSipperDatabase* database);
  ~NetworkEventStorage();

  NetworkEventStorage(const NetworkEventStorage&) = delete;
  NetworkEventStorage& operator=(const NetworkEventStorage&) = delete;

 private:
  DataSipperDatabase* database_;
  base::WeakPtrFactory<NetworkEventStorage> weak_factory_{this};
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_STORAGE_NETWORK_EVENT_STORAGE_H_
