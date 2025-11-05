// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_WORKFLOW_NODES_FILTER_NODE_H_
#define COMPONENTS_DATASIPPER_WORKFLOW_NODES_FILTER_NODE_H_

#include <string>
#include <vector>

#include "base/values.h"

namespace datasipper {

// Filters data based on conditional expressions
class FilterNode {
 public:
  // Comparison operators
  enum class Operator {
    EQUALS,           // ==
    NOT_EQUALS,       // !=
    GREATER_THAN,     // >
    LESS_THAN,        // <
    GREATER_EQUALS,   // >=
    LESS_EQUALS,      // <=
    CONTAINS,         // string/array contains
    REGEX_MATCH,      // regex pattern match
    EXISTS,           // field exists
    NOT_EXISTS        // field doesn't exist
  };

  // Single condition
  struct Condition {
    Condition();
    Condition(const Condition&);
    Condition& operator=(const Condition&);
    ~Condition();

    std::string field_path;  // JSONPath to field
    Operator op;
    std::string value;       // Comparison value
  };

  // Filter configuration
  struct FilterConfig {
    FilterConfig();
    FilterConfig(const FilterConfig&);
    FilterConfig& operator=(const FilterConfig&);
    ~FilterConfig();

    std::vector<Condition> conditions;
    std::string logic = "AND";  // "AND" or "OR"
    bool invert_result = false;  // NOT operation

    base::Value ToJson() const;
    static FilterConfig FromJson(const base::Value& json);
  };

  FilterNode();
  ~FilterNode();

  FilterNode(const FilterNode&) = delete;
  FilterNode& operator=(const FilterNode&) = delete;

  // Evaluate filter conditions
  bool Evaluate(const base::Value& input, const FilterConfig& config);

 private:
  // Evaluate single condition
  bool EvaluateCondition(const base::Value& input, const Condition& condition);

  // Compare values based on operator
  bool Compare(const base::Value* field_value,
               Operator op,
               const std::string& compare_value);

  // Extract value using JSONPath (simplified)
  const base::Value* ExtractValue(const base::Value& data,
                                   const std::string& path);

  // Convert string to operator
  static Operator StringToOperator(const std::string& op_str);
  static std::string OperatorToString(Operator op);
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_WORKFLOW_NODES_FILTER_NODE_H_
