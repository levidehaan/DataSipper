// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/workflow/nodes/export_node.h"

namespace datasipper {

ExportNode::ExportConfig::ExportConfig() = default;

ExportNode::ExportConfig::ExportConfig(const ExportConfig& other)
    : export_type(other.export_type), settings(other.settings.Clone()) {}

ExportNode::ExportConfig& ExportNode::ExportConfig::operator=(const ExportConfig& other) {
  if (this != &other) {
    export_type = other.export_type;
    settings = other.settings.Clone();
  }
  return *this;
}

ExportNode::ExportConfig::~ExportConfig() = default;

}  // namespace datasipper
