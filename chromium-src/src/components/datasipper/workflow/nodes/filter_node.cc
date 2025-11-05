// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/workflow/nodes/filter_node.h"

#include <regex>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace datasipper {

// Condition implementation
FilterNode::Condition::Condition() : op(Operator::EQUALS) {}
FilterNode::Condition::Condition(const Condition&) = default;
FilterNode::Condition& FilterNode::Condition::operator=(const Condition&) = default;
FilterNode::Condition::~Condition() = default;

// FilterConfig implementation
FilterNode::FilterConfig::FilterConfig() = default;
FilterNode::FilterConfig::FilterConfig(const FilterConfig&) = default;
FilterNode::FilterConfig& FilterNode::FilterConfig::operator=(
    const FilterConfig&) = default;
FilterNode::FilterConfig::~FilterConfig() = default;

base::Value FilterNode::FilterConfig::ToJson() const {
  base::Value::Dict dict;

  base::Value::List conditions_list;
  for (const auto& condition : conditions) {
    base::Value::Dict cond_dict;
    cond_dict.Set("field_path", condition.field_path);
    cond_dict.Set("operator", OperatorToString(condition.op));
    cond_dict.Set("value", condition.value);
    conditions_list.Append(std::move(cond_dict));
  }
  dict.Set("conditions", std::move(conditions_list));
  dict.Set("logic", logic);
  dict.Set("invert_result", invert_result);

  return base::Value(std::move(dict));
}

FilterNode::FilterConfig FilterNode::FilterConfig::FromJson(
    const base::Value& json) {
  FilterConfig config;

  if (!json.is_dict()) {
    return config;
  }

  const base::Value::Dict& dict = json.GetDict();

  if (const base::Value::List* cond_list = dict.FindList("conditions")) {
    for (const auto& cond_value : *cond_list) {
      if (!cond_value.is_dict()) {
        continue;
      }

      const base::Value::Dict& cond_dict = cond_value.GetDict();
      Condition condition;

      if (const std::string* field = cond_dict.FindString("field_path")) {
        condition.field_path = *field;
      }
      if (const std::string* op_str = cond_dict.FindString("operator")) {
        condition.op = StringToOperator(*op_str);
      }
      if (const std::string* val = cond_dict.FindString("value")) {
        condition.value = *val;
      }

      config.conditions.push_back(std::move(condition));
    }
  }

  if (const std::string* logic_str = dict.FindString("logic")) {
    config.logic = *logic_str;
  }
  config.invert_result = dict.FindBool("invert_result").value_or(false);

  return config;
}

// FilterNode implementation
FilterNode::FilterNode() = default;
FilterNode::~FilterNode() = default;

bool FilterNode::Evaluate(const base::Value& input, const FilterConfig& config) {
  if (config.conditions.empty()) {
    return true;  // No conditions = always pass
  }

  bool result;

  if (config.logic == "AND") {
    // All conditions must be true
    result = true;
    for (const auto& condition : config.conditions) {
      if (!EvaluateCondition(input, condition)) {
        result = false;
        break;
      }
    }
  } else {  // OR
    // At least one condition must be true
    result = false;
    for (const auto& condition : config.conditions) {
      if (EvaluateCondition(input, condition)) {
        result = true;
        break;
      }
    }
  }

  // Apply inversion if configured
  if (config.invert_result) {
    result = !result;
  }

  return result;
}

bool FilterNode::EvaluateCondition(const base::Value& input,
                                   const Condition& condition) {
  const base::Value* field_value = ExtractValue(input, condition.field_path);
  return Compare(field_value, condition.op, condition.value);
}

bool FilterNode::Compare(const base::Value* field_value,
                         Operator op,
                         const std::string& compare_value) {
  // Handle EXISTS and NOT_EXISTS operators
  if (op == Operator::EXISTS) {
    return field_value != nullptr;
  } else if (op == Operator::NOT_EXISTS) {
    return field_value == nullptr;
  }

  // For other operators, field must exist
  if (!field_value) {
    return false;
  }

  switch (op) {
    case Operator::EQUALS: {
      if (field_value->is_string()) {
        return field_value->GetString() == compare_value;
      } else if (field_value->is_int()) {
        int compare_int;
        if (base::StringToInt(compare_value, &compare_int)) {
          return field_value->GetInt() == compare_int;
        }
      } else if (field_value->is_double()) {
        double compare_double;
        if (base::StringToDouble(compare_value, &compare_double)) {
          return field_value->GetDouble() == compare_double;
        }
      } else if (field_value->is_bool()) {
        return field_value->GetBool() == (compare_value == "true");
      }
      return false;
    }

    case Operator::NOT_EQUALS: {
      return !Compare(field_value, Operator::EQUALS, compare_value);
    }

    case Operator::GREATER_THAN: {
      if (field_value->is_int()) {
        int compare_int;
        if (base::StringToInt(compare_value, &compare_int)) {
          return field_value->GetInt() > compare_int;
        }
      } else if (field_value->is_double()) {
        double compare_double;
        if (base::StringToDouble(compare_value, &compare_double)) {
          return field_value->GetDouble() > compare_double;
        }
      }
      return false;
    }

    case Operator::LESS_THAN: {
      if (field_value->is_int()) {
        int compare_int;
        if (base::StringToInt(compare_value, &compare_int)) {
          return field_value->GetInt() < compare_int;
        }
      } else if (field_value->is_double()) {
        double compare_double;
        if (base::StringToDouble(compare_value, &compare_double)) {
          return field_value->GetDouble() < compare_double;
        }
      }
      return false;
    }

    case Operator::GREATER_EQUALS: {
      if (field_value->is_int()) {
        int compare_int;
        if (base::StringToInt(compare_value, &compare_int)) {
          return field_value->GetInt() >= compare_int;
        }
      } else if (field_value->is_double()) {
        double compare_double;
        if (base::StringToDouble(compare_value, &compare_double)) {
          return field_value->GetDouble() >= compare_double;
        }
      }
      return false;
    }

    case Operator::LESS_EQUALS: {
      if (field_value->is_int()) {
        int compare_int;
        if (base::StringToInt(compare_value, &compare_int)) {
          return field_value->GetInt() <= compare_int;
        }
      } else if (field_value->is_double()) {
        double compare_double;
        if (base::StringToDouble(compare_value, &compare_double)) {
          return field_value->GetDouble() <= compare_double;
        }
      }
      return false;
    }

    case Operator::CONTAINS: {
      if (field_value->is_string()) {
        return field_value->GetString().find(compare_value) != std::string::npos;
      } else if (field_value->is_list()) {
        for (const auto& item : field_value->GetList()) {
          if (item.is_string() && item.GetString() == compare_value) {
            return true;
          }
        }
      }
      return false;
    }

    case Operator::REGEX_MATCH: {
      if (field_value->is_string()) {
        // Chromium disables exceptions, so we can't catch regex_error
        // Use a simpler substring match as fallback
        // TODO: Consider using RE2 library which is used in Chromium
        return field_value->GetString().find(compare_value) != std::string::npos;
      }
      return false;
    }

    default:
      return false;
  }
}

const base::Value* FilterNode::ExtractValue(const base::Value& data,
                                            const std::string& path) {
  // Same implementation as TransformNode::ExtractValue
  if (path.empty()) {
    return &data;
  }

  std::string normalized_path = path;
  if (base::StartsWith(normalized_path, "$.")) {
    normalized_path = normalized_path.substr(2);
  } else if (base::StartsWith(normalized_path, "$")) {
    normalized_path = normalized_path.substr(1);
  }

  if (normalized_path.empty()) {
    return &data;
  }

  std::vector<std::string> parts = base::SplitString(
      normalized_path, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  const base::Value* current = &data;

  for (const auto& part : parts) {
    if (!current) {
      return nullptr;
    }

    size_t bracket_pos = part.find('[');
    if (bracket_pos != std::string::npos) {
      std::string field_name = part.substr(0, bracket_pos);

      if (current->is_dict()) {
        current = current->GetDict().Find(field_name);
        if (!current) {
          return nullptr;
        }
      }

      size_t close_bracket = part.find(']', bracket_pos);
      if (close_bracket != std::string::npos) {
        std::string index_str = part.substr(bracket_pos + 1, close_bracket - bracket_pos - 1);
        int index;
        if (base::StringToInt(index_str, &index) && current->is_list()) {
          const base::Value::List& list = current->GetList();
          if (index >= 0 && static_cast<size_t>(index) < list.size()) {
            current = &list[index];
          } else {
            return nullptr;
          }
        } else {
          return nullptr;
        }
      }
    } else {
      if (current->is_dict()) {
        current = current->GetDict().Find(part);
      } else {
        return nullptr;
      }
    }
  }

  return current;
}

FilterNode::Operator FilterNode::StringToOperator(const std::string& op_str) {
  if (op_str == "==" || op_str == "equals") return Operator::EQUALS;
  if (op_str == "!=" || op_str == "not_equals") return Operator::NOT_EQUALS;
  if (op_str == ">" || op_str == "greater_than") return Operator::GREATER_THAN;
  if (op_str == "<" || op_str == "less_than") return Operator::LESS_THAN;
  if (op_str == ">=" || op_str == "greater_equals") return Operator::GREATER_EQUALS;
  if (op_str == "<=" || op_str == "less_equals") return Operator::LESS_EQUALS;
  if (op_str == "contains") return Operator::CONTAINS;
  if (op_str == "regex" || op_str == "regex_match") return Operator::REGEX_MATCH;
  if (op_str == "exists") return Operator::EXISTS;
  if (op_str == "not_exists") return Operator::NOT_EXISTS;
  return Operator::EQUALS;  // default
}

std::string FilterNode::OperatorToString(Operator op) {
  switch (op) {
    case Operator::EQUALS: return "==";
    case Operator::NOT_EQUALS: return "!=";
    case Operator::GREATER_THAN: return ">";
    case Operator::LESS_THAN: return "<";
    case Operator::GREATER_EQUALS: return ">=";
    case Operator::LESS_EQUALS: return "<=";
    case Operator::CONTAINS: return "contains";
    case Operator::REGEX_MATCH: return "regex";
    case Operator::EXISTS: return "exists";
    case Operator::NOT_EXISTS: return "not_exists";
  }
  return "==";
}

}  // namespace datasipper
