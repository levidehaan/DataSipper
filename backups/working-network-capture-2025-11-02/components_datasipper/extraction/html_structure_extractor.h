// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_EXTRACTION_HTML_STRUCTURE_EXTRACTOR_H_
#define COMPONENTS_DATASIPPER_EXTRACTION_HTML_STRUCTURE_EXTRACTOR_H_

#include <string>
#include <vector>

#include "components/datasipper/extraction/extraction_result.h"

namespace content {
class WebContents;
}  // namespace content

namespace datasipper {

// Extracts structured data from HTML elements (tables, lists, forms, etc.)
class HtmlStructureExtractor {
 public:
  HtmlStructureExtractor();
  ~HtmlStructureExtractor();

  HtmlStructureExtractor(const HtmlStructureExtractor&) = delete;
  HtmlStructureExtractor& operator=(const HtmlStructureExtractor&) = delete;

  // Extract all structured HTML from the page
  std::vector<ExtractedDataSource> Extract(content::WebContents* web_contents);

  // Extract specific structure types
  std::vector<ExtractedDataSource> ExtractTables(const std::string& html);
  std::vector<ExtractedDataSource> ExtractLists(const std::string& html);
  std::vector<ExtractedDataSource> ExtractForms(const std::string& html);
  std::vector<ExtractedDataSource> ExtractDefinitionLists(
      const std::string& html);

 private:
  // Parse HTML table to structured data
  base::Value ParseTable(const std::string& table_html);

  // Parse list (ul/ol) to structured data
  base::Value ParseList(const std::string& list_html);

  // Parse form to field definitions
  base::Value ParseForm(const std::string& form_html);

  // Parse definition list (dl) to key-value pairs
  base::Value ParseDefinitionList(const std::string& dl_html);

  // Generate CSS selector for element
  std::string GenerateSelector(const std::string& element_html);

  // Clean and normalize text content
  std::string CleanText(const std::string& text);

  // Detect if table has headers
  bool HasTableHeaders(const std::string& table_html);

  // Extract table headers
  std::vector<std::string> ExtractTableHeaders(const std::string& table_html);
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_EXTRACTION_HTML_STRUCTURE_EXTRACTOR_H_
