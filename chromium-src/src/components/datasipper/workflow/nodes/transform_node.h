// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_WORKFLOW_NODES_TRANSFORM_NODE_H_
#define COMPONENTS_DATASIPPER_WORKFLOW_NODES_TRANSFORM_NODE_H_

#include <string>
#include <vector>

#include "base/values.h"

namespace datasipper {

// Transforms data using field mapping, type conversion, and JSONPath queries
class TransformNode {
 public:
  // Field transformation configuration
  struct FieldMapping {
    FieldMapping();
    FieldMapping(const FieldMapping&);
    FieldMapping& operator=(const FieldMapping&);
    ~FieldMapping();

    std::string source_path;     // JSONPath to source field (e.g., "$.data.title")
    std::string target_field;    // Target field name
    std::string type;            // "string", "number", "boolean", "array", "object"
    std::string default_value;   // Default if source not found
    bool required = false;       // Fail transform if missing
  };

  // Transform configuration
  struct TransformConfig {
    TransformConfig();
    TransformConfig(const TransformConfig&);
    TransformConfig& operator=(const TransformConfig&);
    ~TransformConfig();

    std::vector<FieldMapping> mappings;
    bool remove_null_fields = false;
    bool flatten_arrays = false;

    base::Value ToJson() const;
    static TransformConfig FromJson(const base::Value& json);
  };

  TransformNode();
  ~TransformNode();

  TransformNode(const TransformNode&) = delete;
  TransformNode& operator=(const TransformNode&) = delete;

  // Transform input data according to configuration
  base::Value Transform(const base::Value& input, const TransformConfig& config);

 private:
  // Extract value using JSONPath (simplified implementation)
  const base::Value* ExtractValue(const base::Value& data,
                                   const std::string& path);

  // Convert value to target type
  base::Value ConvertType(const base::Value& value, const std::string& target_type);

  // Flatten nested arrays
  base::Value FlattenArrays(const base::Value& value);
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_WORKFLOW_NODES_TRANSFORM_NODE_H_
