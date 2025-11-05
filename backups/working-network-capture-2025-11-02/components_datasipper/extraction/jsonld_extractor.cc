// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/extraction/jsonld_extractor.h"

#include <optional>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace datasipper {

namespace {

// Extract @type from JSON-LD object
std::string GetTypeFromJsonLd(const base::Value& jsonld) {
  if (!jsonld.is_dict()) {
    return std::string();
  }

  const base::Value::Dict& dict = jsonld.GetDict();

  // Check for @type
  if (const std::string* type = dict.FindString("@type")) {
    return *type;
  }

  // Sometimes it's just "type" without @
  if (const std::string* type = dict.FindString("type")) {
    return *type;
  }

  return std::string();
}

// Expand @context if it's a URL reference
// For now, we'll just return the value as-is
// Full JSON-LD expansion would require fetching remote contexts
base::Value ExpandJsonLdContext(const base::Value& jsonld) {
  // TODO: Implement full JSON-LD context expansion
  // This would involve:
  // 1. Fetching @context URLs
  // 2. Resolving term mappings
  // 3. Expanding compact IRIs

  // For now, just return a copy
  return jsonld.Clone();
}

}  // namespace

JsonLdExtractor::JsonLdExtractor() = default;
JsonLdExtractor::~JsonLdExtractor() = default;

std::vector<ExtractedDataSource> JsonLdExtractor::ExtractFromHtml(
    const std::string& html) {
  std::vector<ExtractedDataSource> results;

  // Find all <script type="application/ld+json"> tags
  size_t pos = 0;
  while (true) {
    size_t script_start = html.find("<script", pos);
    if (script_start == std::string::npos) {
      break;
    }

    // Check if this is a JSON-LD script tag
    size_t type_pos = html.find("type=", script_start);
    size_t tag_end = html.find(">", script_start);

    if (type_pos == std::string::npos || type_pos > tag_end) {
      pos = script_start + 7;
      continue;
    }

    // Look for application/ld+json
    size_t jsonld_pos = html.find("application/ld+json", type_pos);
    if (jsonld_pos == std::string::npos || jsonld_pos > tag_end) {
      pos = script_start + 7;
      continue;
    }

    // Extract content
    size_t content_start = tag_end + 1;
    size_t content_end = html.find("</script>", content_start);
    if (content_end == std::string::npos) {
      break;
    }

    std::string jsonld_content = html.substr(
        content_start, content_end - content_start);
    base::TrimWhitespaceASCII(jsonld_content, base::TRIM_ALL, &jsonld_content);

    // Parse JSON-LD
    std::optional<base::Value> parsed = base::JSONReader::Read(jsonld_content);
    if (!parsed) {
      pos = content_end + 9;
      continue;
    }

    // Handle both single objects and arrays of JSON-LD objects
    std::vector<base::Value> jsonld_objects;
    if (parsed->is_list()) {
      for (const auto& item : parsed->GetList()) {
        jsonld_objects.push_back(item.Clone());
      }
    } else if (parsed->is_dict()) {
      jsonld_objects.push_back(std::move(*parsed));
    }

    // Process each JSON-LD object
    for (auto& jsonld_obj : jsonld_objects) {
      if (!jsonld_obj.is_dict()) {
        continue;
      }

      ExtractedDataSource source;
      source.type = DataSourceType::JSON_LD;
      source.selector_path = base::StringPrintf(
          "script[type='application/ld+json']:nth(%zu)", results.size());

      std::string schema_type = GetTypeFromJsonLd(jsonld_obj);
      source.label = schema_type.empty()
          ? base::StringPrintf("JSON-LD Data %zu", results.size() + 1)
          : base::StringPrintf("JSON-LD: %s", schema_type.c_str());

      source.raw_content = jsonld_content;
      source.parsed_data = std::move(jsonld_obj);
      source.confidence_score = 1.0f;

      // Store metadata
      if (!schema_type.empty()) {
        source.metadata["schema_type"] = schema_type;
      }

      // Extract common schema.org properties
      const base::Value::Dict& dict = source.parsed_data.GetDict();

      if (const std::string* name = dict.FindString("name")) {
        source.metadata["name"] = *name;
      }
      if (const std::string* url = dict.FindString("url")) {
        source.metadata["url"] = *url;
      }
      if (const std::string* context = dict.FindString("@context")) {
        source.metadata["context"] = *context;
      }

      results.push_back(std::move(source));
    }

    pos = content_end + 9;  // Skip past </script>
  }

  return results;
}

// static
base::Value JsonLdExtractor::ParseJsonLd(const std::string& jsonld_str) {
  std::optional<base::Value> result = base::JSONReader::Read(jsonld_str);
  return result ? std::move(*result) : base::Value();
}

// static
std::string JsonLdExtractor::GetSchemaType(const base::Value& jsonld) {
  return GetTypeFromJsonLd(jsonld);
}

base::Value JsonLdExtractor::ExpandContext(const base::Value& jsonld) {
  return ExpandJsonLdContext(jsonld);
}

}  // namespace datasipper
