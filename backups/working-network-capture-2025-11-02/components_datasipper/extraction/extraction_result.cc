// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/extraction/extraction_result.h"

#include <set>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/uuid.h"

namespace datasipper {

// ExtractedDataSource

ExtractedDataSource::ExtractedDataSource() {
  id = base::Uuid::GenerateRandomV4().AsLowercaseString();
}

ExtractedDataSource::ExtractedDataSource(const ExtractedDataSource& other)
    : id(other.id),
      type(other.type),
      selector_path(other.selector_path),
      label(other.label),
      raw_content(other.raw_content),
      parsed_data(other.parsed_data.Clone()),
      metadata(other.metadata),
      extracted_at(other.extracted_at),
      confidence_score(other.confidence_score),
      is_tracked(other.is_tracked) {}

ExtractedDataSource::ExtractedDataSource(ExtractedDataSource&&) = default;

ExtractedDataSource& ExtractedDataSource::operator=(
    const ExtractedDataSource& other) {
  if (this != &other) {
    id = other.id;
    type = other.type;
    selector_path = other.selector_path;
    label = other.label;
    raw_content = other.raw_content;
    parsed_data = other.parsed_data.Clone();
    metadata = other.metadata;
    extracted_at = other.extracted_at;
    confidence_score = other.confidence_score;
    is_tracked = other.is_tracked;
  }
  return *this;
}

ExtractedDataSource& ExtractedDataSource::operator=(ExtractedDataSource&&) =
    default;

ExtractedDataSource::~ExtractedDataSource() = default;

// PageExtractionResult

PageExtractionResult::PageExtractionResult() = default;

PageExtractionResult::PageExtractionResult(const PageExtractionResult&) =
    default;

PageExtractionResult& PageExtractionResult::operator=(
    const PageExtractionResult&) = default;

PageExtractionResult::~PageExtractionResult() = default;

// DataSourceSchema::Field

DataSourceSchema::Field::Field() = default;

DataSourceSchema::Field::Field(const Field& other)
    : name(other.name),
      type(other.type),
      is_required(other.is_required),
      description(other.description),
      default_value(other.default_value.Clone()) {}

DataSourceSchema::Field::Field(Field&&) = default;

DataSourceSchema::Field& DataSourceSchema::Field::operator=(const Field& other) {
  if (this != &other) {
    name = other.name;
    type = other.type;
    is_required = other.is_required;
    description = other.description;
    default_value = other.default_value.Clone();
  }
  return *this;
}

DataSourceSchema::Field& DataSourceSchema::Field::operator=(Field&&) = default;

DataSourceSchema::Field::~Field() = default;

// DataSourceSchema

DataSourceSchema::DataSourceSchema() = default;

DataSourceSchema::DataSourceSchema(const DataSourceSchema&) = default;

DataSourceSchema& DataSourceSchema::operator=(const DataSourceSchema&) =
    default;

DataSourceSchema::~DataSourceSchema() = default;

base::Value DataSourceSchema::ToJsonSchema() const {
  base::Value::Dict schema;
  schema.Set("type", "object");

  base::Value::Dict properties;
  base::Value::List required;

  for (const auto& field : fields) {
    base::Value::Dict field_schema;
    field_schema.Set("type", field.type);

    if (!field.description.empty()) {
      field_schema.Set("description", field.description);
    }

    if (!field.default_value.is_none()) {
      field_schema.Set("default", field.default_value.Clone());
    }

    properties.Set(field.name, std::move(field_schema));

    if (field.is_required) {
      required.Append(field.name);
    }
  }

  schema.Set("properties", std::move(properties));

  if (!required.empty()) {
    schema.Set("required", std::move(required));
  }

  return base::Value(std::move(schema));
}

// static
DataSourceSchema DataSourceSchema::FromJsonSchema(const base::Value& schema) {
  DataSourceSchema result;

  if (!schema.is_dict()) {
    return result;
  }

  const base::Value::Dict& schema_dict = schema.GetDict();
  const base::Value::Dict* properties = schema_dict.FindDict("properties");
  const base::Value::List* required = schema_dict.FindList("required");

  if (!properties) {
    return result;
  }

  // Build set of required fields for quick lookup
  std::set<std::string> required_fields;
  if (required) {
    for (const auto& req : *required) {
      if (req.is_string()) {
        required_fields.insert(req.GetString());
      }
    }
  }

  // Parse each property
  for (const auto [field_name, field_value] : *properties) {
    if (!field_value.is_dict()) {
      continue;
    }

    Field field;
    field.name = field_name;

    const base::Value::Dict& field_dict = field_value.GetDict();

    if (const std::string* type = field_dict.FindString("type")) {
      field.type = *type;
    }

    if (const std::string* desc = field_dict.FindString("description")) {
      field.description = *desc;
    }

    if (const base::Value* default_val = field_dict.Find("default")) {
      field.default_value = default_val->Clone();
    }

    field.is_required = required_fields.count(field_name) > 0;

    result.fields.push_back(std::move(field));
  }

  return result;
}

}  // namespace datasipper
