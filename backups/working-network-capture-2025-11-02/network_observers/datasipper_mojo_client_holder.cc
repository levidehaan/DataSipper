// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/datasipper_mojo_client_holder.h"

#include "base/logging.h"

namespace network {

DataSipperMojoClientHolder* DataSipperMojoClientHolder::GetInstance() {
  static base::NoDestructor<DataSipperMojoClientHolder> instance;
  return instance.get();
}

DataSipperMojoClientHolder::DataSipperMojoClientHolder() = default;
DataSipperMojoClientHolder::~DataSipperMojoClientHolder() = default;

void DataSipperMojoClientHolder::SetClient(
    mojo::PendingRemote<datasipper::mojom::DataSipperNetworkClient> client) {
  base::AutoLock lock(lock_);
  if (client) {
    // Bind to SharedRemote for thread-safe sharing across URLLoaders
    client_ = mojo::SharedRemote<datasipper::mojom::DataSipperNetworkClient>(
        std::move(client));
    LOG(INFO) << "✅ DataSipperMojoClientHolder: Shared Mojo client bound and ready";
  } else {
    LOG(WARNING) << "⚠️ DataSipperMojoClientHolder: Null client provided";
  }
}

mojo::SharedRemote<datasipper::mojom::DataSipperNetworkClient>
DataSipperMojoClientHolder::GetClient() {
  base::AutoLock lock(lock_);
  // SharedRemote can be copied and shared across multiple consumers safely
  return client_;
}

bool DataSipperMojoClientHolder::HasClient() const {
  base::AutoLock lock(lock_);
  return client_.is_bound();
}

}  // namespace network
