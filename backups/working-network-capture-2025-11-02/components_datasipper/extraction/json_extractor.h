// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_EXTRACTION_JSON_EXTRACTOR_H_
#define COMPONENTS_DATASIPPER_EXTRACTION_JSON_EXTRACTOR_H_

#include <string>
#include <vector>

#include "components/datasipper/extraction/extraction_result.h"

namespace content {
class WebContents;
}  // namespace content

namespace datasipper {

// Extracts JSON data from various page sources
class JsonExtractor {
 public:
  JsonExtractor();
  ~JsonExtractor();

  JsonExtractor(const JsonExtractor&) = delete;
  JsonExtractor& operator=(const JsonExtractor&) = delete;

  // Extract all JSON objects from the page
  std::vector<ExtractedDataSource> Extract(content::WebContents* web_contents);

  // Extract JSON from specific sources
  std::vector<ExtractedDataSource> ExtractFromScriptTags(
      const std::string& page_html);
  std::vector<ExtractedDataSource> ExtractFromDataAttributes(
      const std::string& page_html);
  std::vector<ExtractedDataSource> ExtractFromInlineScripts(
      const std::string& page_html);

  // Parse and validate JSON string
  static bool IsValidJson(const std::string& json_str);
  static base::Value ParseJson(const std::string& json_str);

 private:
  // Find JSON patterns in text
  std::vector<std::string> FindJsonPatterns(const std::string& text);

  // Generate selector for data attribute
  std::string GenerateDataAttributeSelector(const std::string& element_html,
                                             const std::string& attr_name);

  // Infer schema from JSON object
  DataSourceSchema InferSchemaFromJson(const base::Value& json);
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_EXTRACTION_JSON_EXTRACTOR_H_
