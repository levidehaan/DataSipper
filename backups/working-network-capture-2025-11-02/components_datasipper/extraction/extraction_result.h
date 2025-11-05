// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_EXTRACTION_EXTRACTION_RESULT_H_
#define COMPONENTS_DATASIPPER_EXTRACTION_EXTRACTION_RESULT_H_

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"

namespace datasipper {

// Types of data sources that can be extracted from a page
enum class DataSourceType {
  HTML_TABLE,     // HTML <table> elements
  JSON_OBJECT,    // JSON in <script> tags or data attributes
  JSON_LD,        // JSON-LD structured data
  CSV_DATA,       // Embedded CSV/TSV data
  FORM_DATA,      // Form elements and their values
  XML_DATA,       // XML/RSS feeds
  CUSTOM          // User-defined extractors
};

// Represents a single extracted data source from a webpage
struct ExtractedDataSource {
  ExtractedDataSource();
  ExtractedDataSource(const ExtractedDataSource&);
  ExtractedDataSource(ExtractedDataSource&&);
  ExtractedDataSource& operator=(const ExtractedDataSource&);
  ExtractedDataSource& operator=(ExtractedDataSource&&);
  ~ExtractedDataSource();

  // Unique identifier for this data source
  std::string id;

  // Type of data source
  DataSourceType type;

  // CSS selector or XPath to locate the element
  std::string selector_path;

  // Human-readable label (auto-generated or user-defined)
  std::string label;

  // Raw content as extracted from the page
  std::string raw_content;

  // Parsed/structured data (JSON representation)
  base::Value parsed_data;

  // Metadata about the extraction
  std::map<std::string, std::string> metadata;

  // When this data was extracted
  base::Time extracted_at;

  // Confidence score (0.0 - 1.0) for automatic extraction
  float confidence_score = 1.0;

  // Whether this data source is actively being tracked for changes
  bool is_tracked = false;
};

// Result of a page data detection scan
struct PageExtractionResult {
  PageExtractionResult();
  PageExtractionResult(const PageExtractionResult&);
  PageExtractionResult& operator=(const PageExtractionResult&);
  ~PageExtractionResult();

  // URL of the page that was scanned
  std::string url;

  // All detected data sources
  std::vector<ExtractedDataSource> data_sources;

  // Scan timestamp
  base::Time scanned_at;

  // Any errors or warnings during extraction
  std::vector<std::string> errors;
  std::vector<std::string> warnings;

  // Performance metrics
  int64_t scan_duration_ms = 0;
};

// Schema information for a data source
struct DataSourceSchema {
  DataSourceSchema();
  DataSourceSchema(const DataSourceSchema&);
  DataSourceSchema& operator=(const DataSourceSchema&);
  ~DataSourceSchema();

  // Field definitions
  struct Field {
    Field();
    Field(const Field&);
    Field(Field&&);
    Field& operator=(const Field&);
    Field& operator=(Field&&);
    ~Field();

    std::string name;
    std::string type;  // "string", "number", "boolean", "object", "array"
    bool is_required = false;
    std::string description;
    base::Value default_value;
  };

  std::vector<Field> fields;

  // JSON Schema representation
  base::Value ToJsonSchema() const;
  static DataSourceSchema FromJsonSchema(const base::Value& schema);
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_EXTRACTION_EXTRACTION_RESULT_H_
