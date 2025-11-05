// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_EXTRACTION_JSONLD_EXTRACTOR_H_
#define COMPONENTS_DATASIPPER_EXTRACTION_JSONLD_EXTRACTOR_H_

#include <string>
#include <vector>

#include "components/datasipper/extraction/extraction_result.h"

namespace content {
class WebContents;
}  // namespace content

namespace datasipper {

// Extracts JSON-LD structured data from web pages
// JSON-LD is commonly used for SEO and schema.org markup
class JsonLdExtractor {
 public:
  JsonLdExtractor();
  ~JsonLdExtractor();

  JsonLdExtractor(const JsonLdExtractor&) = delete;
  JsonLdExtractor& operator=(const JsonLdExtractor&) = delete;

  // Extract all JSON-LD objects from the page
  std::vector<ExtractedDataSource> Extract(content::WebContents* web_contents);

  // Extract from HTML string
  std::vector<ExtractedDataSource> ExtractFromHtml(const std::string& html);

  // Parse JSON-LD and normalize it
  static base::Value ParseJsonLd(const std::string& jsonld_str);

  // Get schema.org type from JSON-LD object
  static std::string GetSchemaType(const base::Value& jsonld);

 private:
  // Find all <script type="application/ld+json"> tags
  std::vector<std::string> FindJsonLdScripts(const std::string& html);

  // Generate human-readable label from schema type
  std::string GenerateLabelFromType(const std::string& schema_type);

  // Expand @context URLs if needed (simplified version)
  base::Value ExpandContext(const base::Value& jsonld);
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_EXTRACTION_JSONLD_EXTRACTOR_H_
