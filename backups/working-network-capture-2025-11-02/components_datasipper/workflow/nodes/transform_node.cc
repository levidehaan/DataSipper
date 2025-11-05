// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/workflow/nodes/transform_node.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace datasipper {

// FieldMapping implementation
TransformNode::FieldMapping::FieldMapping() = default;
TransformNode::FieldMapping::FieldMapping(const FieldMapping&) = default;
TransformNode::FieldMapping& TransformNode::FieldMapping::operator=(
    const FieldMapping&) = default;
TransformNode::FieldMapping::~FieldMapping() = default;

// TransformConfig implementation
TransformNode::TransformConfig::TransformConfig() = default;
TransformNode::TransformConfig::TransformConfig(const TransformConfig&) = default;
TransformNode::TransformConfig& TransformNode::TransformConfig::operator=(
    const TransformConfig&) = default;
TransformNode::TransformConfig::~TransformConfig() = default;

base::Value TransformNode::TransformConfig::ToJson() const {
  base::Value::Dict dict;

  base::Value::List mappings_list;
  for (const auto& mapping : mappings) {
    base::Value::Dict mapping_dict;
    mapping_dict.Set("source_path", mapping.source_path);
    mapping_dict.Set("target_field", mapping.target_field);
    mapping_dict.Set("type", mapping.type);
    if (!mapping.default_value.empty()) {
      mapping_dict.Set("default_value", mapping.default_value);
    }
    mapping_dict.Set("required", mapping.required);
    mappings_list.Append(std::move(mapping_dict));
  }
  dict.Set("mappings", std::move(mappings_list));
  dict.Set("remove_null_fields", remove_null_fields);
  dict.Set("flatten_arrays", flatten_arrays);

  return base::Value(std::move(dict));
}

TransformNode::TransformConfig TransformNode::TransformConfig::FromJson(
    const base::Value& json) {
  TransformConfig config;

  if (!json.is_dict()) {
    return config;
  }

  const base::Value::Dict& dict = json.GetDict();

  if (const base::Value::List* mappings_list = dict.FindList("mappings")) {
    for (const auto& mapping_value : *mappings_list) {
      if (!mapping_value.is_dict()) {
        continue;
      }

      const base::Value::Dict& mapping_dict = mapping_value.GetDict();
      FieldMapping mapping;

      if (const std::string* source = mapping_dict.FindString("source_path")) {
        mapping.source_path = *source;
      }
      if (const std::string* target = mapping_dict.FindString("target_field")) {
        mapping.target_field = *target;
      }
      if (const std::string* type = mapping_dict.FindString("type")) {
        mapping.type = *type;
      }
      if (const std::string* default_val = mapping_dict.FindString("default_value")) {
        mapping.default_value = *default_val;
      }
      mapping.required = mapping_dict.FindBool("required").value_or(false);

      config.mappings.push_back(std::move(mapping));
    }
  }

  config.remove_null_fields = dict.FindBool("remove_null_fields").value_or(false);
  config.flatten_arrays = dict.FindBool("flatten_arrays").value_or(false);

  return config;
}

// TransformNode implementation
TransformNode::TransformNode() = default;
TransformNode::~TransformNode() = default;

base::Value TransformNode::Transform(const base::Value& input,
                                     const TransformConfig& config) {
  base::Value::Dict result;

  // Apply each field mapping
  for (const auto& mapping : config.mappings) {
    const base::Value* source_value = ExtractValue(input, mapping.source_path);

    if (!source_value) {
      if (mapping.required) {
        LOG(ERROR) << "Required field not found: " << mapping.source_path;
        return base::Value();  // Return null on required field missing
      }

      // Use default value if provided
      if (!mapping.default_value.empty()) {
        result.Set(mapping.target_field, mapping.default_value);
      } else if (!config.remove_null_fields) {
        result.Set(mapping.target_field, base::Value());
      }
      continue;
    }

    // Convert to target type if specified
    base::Value converted_value;
    if (!mapping.type.empty()) {
      converted_value = ConvertType(*source_value, mapping.type);
    } else {
      converted_value = source_value->Clone();
    }

    // Skip null values if configured
    if (config.remove_null_fields && converted_value.is_none()) {
      continue;
    }

    result.Set(mapping.target_field, std::move(converted_value));
  }

  base::Value output(std::move(result));

  // Flatten arrays if configured
  if (config.flatten_arrays) {
    output = FlattenArrays(output);
  }

  return output;
}

const base::Value* TransformNode::ExtractValue(const base::Value& data,
                                               const std::string& path) {
  // Simple JSONPath implementation supporting:
  // - "field" or "$.field" - top level field
  // - "$.field.nested" - nested field
  // - "$.array[0]" - array index (basic support)

  if (path.empty()) {
    return &data;
  }

  // Remove leading $. if present
  std::string normalized_path = path;
  if (base::StartsWith(normalized_path, "$.")) {
    normalized_path = normalized_path.substr(2);
  } else if (base::StartsWith(normalized_path, "$")) {
    normalized_path = normalized_path.substr(1);
  }

  if (normalized_path.empty()) {
    return &data;
  }

  // Split path by dots
  std::vector<std::string> parts = base::SplitString(
      normalized_path, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  const base::Value* current = &data;

  for (const auto& part : parts) {
    if (!current) {
      return nullptr;
    }

    // Check for array index: field[0]
    size_t bracket_pos = part.find('[');
    if (bracket_pos != std::string::npos) {
      std::string field_name = part.substr(0, bracket_pos);

      // Get the field
      if (current->is_dict()) {
        current = current->GetDict().Find(field_name);
        if (!current) {
          return nullptr;
        }
      }

      // Extract array index
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
      // Simple field access
      if (current->is_dict()) {
        current = current->GetDict().Find(part);
      } else {
        return nullptr;
      }
    }
  }

  return current;
}

base::Value TransformNode::ConvertType(const base::Value& value,
                                       const std::string& target_type) {
  if (target_type == "string") {
    if (value.is_string()) {
      return value.Clone();
    } else if (value.is_int()) {
      return base::Value(base::NumberToString(value.GetInt()));
    } else if (value.is_double()) {
      return base::Value(base::NumberToString(value.GetDouble()));
    } else if (value.is_bool()) {
      return base::Value(value.GetBool() ? "true" : "false");
    } else {
      std::string json_str;
      base::JSONWriter::Write(value, &json_str);
      return base::Value(json_str);
    }
  } else if (target_type == "number") {
    if (value.is_int()) {
      return value.Clone();
    } else if (value.is_double()) {
      return value.Clone();
    } else if (value.is_string()) {
      int int_val;
      double double_val;
      if (base::StringToInt(value.GetString(), &int_val)) {
        return base::Value(int_val);
      } else if (base::StringToDouble(value.GetString(), &double_val)) {
        return base::Value(double_val);
      }
    }
  } else if (target_type == "boolean") {
    if (value.is_bool()) {
      return value.Clone();
    } else if (value.is_int()) {
      return base::Value(value.GetInt() != 0);
    } else if (value.is_string()) {
      const std::string& str = value.GetString();
      return base::Value(str == "true" || str == "1" || str == "yes");
    }
  } else if (target_type == "array") {
    if (value.is_list()) {
      return value.Clone();
    } else {
      // Wrap single value in array
      base::Value::List list;
      list.Append(value.Clone());
      return base::Value(std::move(list));
    }
  } else if (target_type == "object") {
    if (value.is_dict()) {
      return value.Clone();
    }
  }

  return value.Clone();
}

base::Value TransformNode::FlattenArrays(const base::Value& value) {
  if (value.is_list()) {
    base::Value::List flattened;
    for (const auto& item : value.GetList()) {
      if (item.is_list()) {
        base::Value flattened_item = FlattenArrays(item);
        for (auto& sub_item : flattened_item.GetList()) {
          flattened.Append(std::move(sub_item));
        }
      } else {
        flattened.Append(item.Clone());
      }
    }
    return base::Value(std::move(flattened));
  } else if (value.is_dict()) {
    base::Value::Dict result;
    for (const auto [key, val] : value.GetDict()) {
      result.Set(key, FlattenArrays(val));
    }
    return base::Value(std::move(result));
  }

  return value.Clone();
}

}  // namespace datasipper
