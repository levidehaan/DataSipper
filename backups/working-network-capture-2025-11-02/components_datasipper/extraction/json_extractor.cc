// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/extraction/json_extractor.h"

#include <optional>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace datasipper {

namespace {

// Recursively infer schema from a JSON value
void InferSchemaFromValueRecursive(const base::Value& value,
                                   std::vector<DataSourceSchema::Field>& fields,
                                   const std::string& prefix = "") {
  if (value.is_dict()) {
    for (const auto [key, val] : value.GetDict()) {
      DataSourceSchema::Field field;
      field.name = prefix.empty() ? key : prefix + "." + key;

      if (val.is_bool()) {
        field.type = "boolean";
      } else if (val.is_int() || val.is_double()) {
        field.type = "number";
      } else if (val.is_string()) {
        field.type = "string";
      } else if (val.is_dict()) {
        field.type = "object";
        InferSchemaFromValueRecursive(val, fields, field.name);
      } else if (val.is_list()) {
        field.type = "array";
      } else {
        field.type = "null";
      }

      fields.push_back(std::move(field));
    }
  }
}

}  // namespace

JsonExtractor::JsonExtractor() = default;
JsonExtractor::~JsonExtractor() = default;

std::vector<ExtractedDataSource> JsonExtractor::ExtractFromScriptTags(
    const std::string& page_html) {
  std::vector<ExtractedDataSource> results;

  // Simple pattern matching for <script type="application/json">
  // In production, would use proper HTML parser
  size_t pos = 0;
  while (true) {
    size_t script_start = page_html.find("<script", pos);
    if (script_start == std::string::npos) {
      break;
    }

    size_t type_pos = page_html.find("type=", script_start);
    if (type_pos == std::string::npos || type_pos > page_html.find(">", script_start)) {
      pos = script_start + 7;
      continue;
    }

    // Check if type is application/json
    size_t json_pos = page_html.find("application/json", type_pos);
    size_t tag_end = page_html.find(">", script_start);
    if (json_pos == std::string::npos || json_pos > tag_end) {
      pos = script_start + 7;
      continue;
    }

    // Extract content between tags
    size_t content_start = tag_end + 1;
    size_t content_end = page_html.find("</script>", content_start);
    if (content_end == std::string::npos) {
      break;
    }

    std::string json_content = page_html.substr(
        content_start, content_end - content_start);
    base::TrimWhitespaceASCII(json_content, base::TRIM_ALL, &json_content);

    // Try to parse as JSON
    std::optional<base::Value> parsed = base::JSONReader::Read(json_content);
    if (parsed && (parsed->is_dict() || parsed->is_list())) {
      ExtractedDataSource source;
      source.type = DataSourceType::JSON_OBJECT;
      source.selector_path = base::StringPrintf("script[type='application/json']:nth(%zu)",
                                                results.size());
      source.label = base::StringPrintf("JSON Data %zu", results.size() + 1);
      source.raw_content = json_content;
      source.parsed_data = std::move(*parsed);
      source.confidence_score = 1.0f;

      // Extract ID if present in data
      if (source.parsed_data.is_dict()) {
        const base::Value::Dict& dict = source.parsed_data.GetDict();
        if (const std::string* id = dict.FindString("id")) {
          source.metadata["data_id"] = *id;
        }
        if (const std::string* type = dict.FindString("type")) {
          source.metadata["data_type"] = *type;
        }
      }

      results.push_back(std::move(source));
    }

    pos = content_end + 9;  // Skip past </script>
  }

  return results;
}

std::vector<ExtractedDataSource> JsonExtractor::ExtractFromDataAttributes(
    const std::string& page_html) {
  std::vector<ExtractedDataSource> results;

  // Find all data-* attributes that contain JSON objects
  size_t pos = 0;
  while (true) {
    size_t data_pos = page_html.find("data-", pos);
    if (data_pos == std::string::npos) {
      break;
    }

    // Find the attribute name
    size_t name_end = page_html.find("=", data_pos);
    if (name_end == std::string::npos) {
      break;
    }

    std::string attr_name = page_html.substr(data_pos, name_end - data_pos);

    // Find the value (handle both single and double quotes)
    size_t value_start = name_end + 1;
    char quote_char = page_html[value_start];
    if (quote_char != '"' && quote_char != '\'') {
      pos = data_pos + 5;
      continue;
    }

    size_t value_end = page_html.find(quote_char, value_start + 1);
    if (value_end == std::string::npos) {
      break;
    }

    std::string value = page_html.substr(value_start + 1,
                                         value_end - value_start - 1);

    // Check if value looks like JSON
    if (value.empty() || (value[0] != '{' && value[0] != '[')) {
      pos = value_end + 1;
      continue;
    }

    // Try to parse as JSON
    std::optional<base::Value> parsed = base::JSONReader::Read(value);
    if (parsed && (parsed->is_dict() || parsed->is_list())) {
      ExtractedDataSource source;
      source.type = DataSourceType::JSON_OBJECT;
      source.selector_path = base::StringPrintf("[%s]", attr_name.c_str());
      source.label = base::StringPrintf("Data Attribute: %s", attr_name.c_str());
      source.raw_content = value;
      source.parsed_data = std::move(*parsed);
      source.confidence_score = 0.9f;  // Slightly lower confidence
      source.metadata["attribute_name"] = attr_name;

      results.push_back(std::move(source));
    }

    pos = value_end + 1;
  }

  return results;
}

// static
bool JsonExtractor::IsValidJson(const std::string& json_str) {
  std::optional<base::Value> result = base::JSONReader::Read(json_str);
  return result.has_value();
}

// static
base::Value JsonExtractor::ParseJson(const std::string& json_str) {
  std::optional<base::Value> result = base::JSONReader::Read(json_str);
  return result ? std::move(*result) : base::Value();
}

DataSourceSchema JsonExtractor::InferSchemaFromJson(const base::Value& json) {
  DataSourceSchema schema;

  if (json.is_dict()) {
    InferSchemaFromValueRecursive(json, schema.fields);
  } else if (json.is_list()) {
    // For arrays, infer schema from first element
    const base::Value::List& list = json.GetList();
    if (!list.empty() && list[0].is_dict()) {
      InferSchemaFromValueRecursive(list[0], schema.fields, "item");
    }
  }

  return schema;
}

}  // namespace datasipper
