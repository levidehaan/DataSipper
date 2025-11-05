// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DATASIPPER_MOJO_CLIENT_HOLDER_H_
#define SERVICES_NETWORK_DATASIPPER_MOJO_CLIENT_HOLDER_H_

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "components/datasipper/mojom/network_observer.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace network {

// Singleton that holds the shared Mojo remote for sending network data from the
// network service to the browser process. This is set once during initialization
// and shared by all URLLoader instances using SharedRemote for thread-safety.
class DataSipperMojoClientHolder {
 public:
  static DataSipperMojoClientHolder* GetInstance();

  // Set the Mojo client remote (called from browser process during init)
  void SetClient(
      mojo::PendingRemote<datasipper::mojom::DataSipperNetworkClient> client);

  // Get a clone of the shared remote for use by URLLoader
  // Returns a valid SharedRemote that can be used to send data
  mojo::SharedRemote<datasipper::mojom::DataSipperNetworkClient> GetClient();

  // Check if a client is available
  bool HasClient() const;

 private:
  friend class base::NoDestructor<DataSipperMojoClientHolder>;

  DataSipperMojoClientHolder();
  ~DataSipperMojoClientHolder();

  mutable base::Lock lock_;
  mojo::SharedRemote<datasipper::mojom::DataSipperNetworkClient> client_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_DATASIPPER_MOJO_CLIENT_HOLDER_H_
