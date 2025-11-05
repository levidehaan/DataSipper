// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_WORKFLOW_NODES_EXPORT_NODE_H_
#define COMPONENTS_DATASIPPER_WORKFLOW_NODES_EXPORT_NODE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/values.h"

namespace datasipper {

// Base class for all export nodes
class ExportNode {
 public:
  using ExportCallback = base::OnceCallback<void(bool success, std::string error)>;

  // Export configuration
  struct ExportConfig {
    ExportConfig();
    ExportConfig(const ExportConfig&);
    ExportConfig& operator=(const ExportConfig&);
    ~ExportConfig();

    std::string export_type;  // "kafka", "redis", "postgresql", etc.
    base::Value::Dict settings;  // Type-specific settings
  };

  virtual ~ExportNode() = default;

  // Export data to external system
  virtual void Export(const base::Value& data,
                     const ExportConfig& config,
                     ExportCallback callback) = 0;

  // Test connection to export destination
  virtual void TestConnection(const ExportConfig& config,
                             base::OnceCallback<void(bool success)> callback) = 0;
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_WORKFLOW_NODES_EXPORT_NODE_H_
