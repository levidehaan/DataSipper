// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/common/network_event.h"

namespace datasipper {

NetworkEvent::NetworkEvent() = default;
NetworkEvent::~NetworkEvent() = default;

NetworkEvent::NetworkEvent(const NetworkEvent&) = default;
NetworkEvent& NetworkEvent::operator=(const NetworkEvent&) = default;

NetworkEvent::NetworkEvent(NetworkEvent&& other) = default;
NetworkEvent& NetworkEvent::operator=(NetworkEvent&& other) = default;

}  // namespace datasipper
