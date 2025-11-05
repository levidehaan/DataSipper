// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_EXTRACTION_CSV_TABLE_EXTRACTOR_H_
#define COMPONENTS_DATASIPPER_EXTRACTION_CSV_TABLE_EXTRACTOR_H_

#include <string>
#include <vector>

#include "components/datasipper/extraction/extraction_result.h"

namespace content {
class WebContents;
}  // namespace content

namespace datasipper {

// Extracts CSV and TSV data embedded in web pages
class CsvTableExtractor {
 public:
  CsvTableExtractor();
  ~CsvTableExtractor();

  CsvTableExtractor(const CsvTableExtractor&) = delete;
  CsvTableExtractor& operator=(const CsvTableExtractor&) = delete;

  // Extract all CSV/TSV data from the page
  std::vector<ExtractedDataSource> Extract(content::WebContents* web_contents);

  // Extract from specific sources
  std::vector<ExtractedDataSource> ExtractFromTextContent(
      const std::string& text);
  std::vector<ExtractedDataSource> ExtractFromPreTags(const std::string& html);
  std::vector<ExtractedDataSource> ExtractFromCodeBlocks(
      const std::string& html);

  // Parse CSV/TSV to structured data
  static base::Value ParseCsv(const std::string& csv_str, char delimiter = ',');
  static base::Value ParseTsv(const std::string& tsv_str);

 private:
  // Detect if text block contains CSV data
  bool IsCsvData(const std::string& text);
  bool IsTsvData(const std::string& text);

  // Auto-detect delimiter
  char DetectDelimiter(const std::string& text);

  // Detect if first row contains headers
  bool HasHeaders(const std::vector<std::vector<std::string>>& rows);

  // Parse CSV line (handles quoted fields)
  std::vector<std::string> ParseCsvLine(const std::string& line,
                                        char delimiter);

  // Clean field value
  std::string CleanField(const std::string& field);

  // Convert parsed CSV to base::Value
  base::Value ConvertToValue(
      const std::vector<std::vector<std::string>>& rows,
      const std::vector<std::string>& headers);
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_EXTRACTION_CSV_TABLE_EXTRACTOR_H_
