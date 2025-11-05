// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/extraction/csv_table_extractor.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace datasipper {

namespace {

// Check if a character is a potential CSV delimiter
bool IsPotentialDelimiter(char c) {
  return c == ',' || c == '\t' || c == ';' || c == '|';
}

// Parse a single CSV line handling quoted fields
std::vector<std::string> ParseCsvLineInternal(const std::string& line,
                                              char delimiter) {
  std::vector<std::string> fields;
  std::string current_field;
  bool in_quotes = false;

  for (size_t i = 0; i < line.length(); ++i) {
    char c = line[i];

    if (c == '"') {
      if (in_quotes) {
        // Check for escaped quote ("")
        if (i + 1 < line.length() && line[i + 1] == '"') {
          current_field += '"';
          ++i;  // Skip next quote
        } else {
          in_quotes = false;
        }
      } else {
        in_quotes = true;
      }
    } else if (c == delimiter && !in_quotes) {
      // End of field
      base::TrimWhitespaceASCII(current_field, base::TRIM_ALL, &current_field);
      fields.push_back(current_field);
      current_field.clear();
    } else {
      current_field += c;
    }
  }

  // Add last field
  base::TrimWhitespaceASCII(current_field, base::TRIM_ALL, &current_field);
  fields.push_back(current_field);

  return fields;
}

}  // namespace

CsvTableExtractor::CsvTableExtractor() = default;
CsvTableExtractor::~CsvTableExtractor() = default;

// static
base::Value CsvTableExtractor::ParseCsv(const std::string& csv_str,
                                        char delimiter) {
  std::vector<std::string> lines = base::SplitString(
      csv_str, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (lines.empty()) {
    return base::Value();
  }

  // Parse all rows
  std::vector<std::vector<std::string>> rows;
  for (const auto& line : lines) {
    // Skip empty lines
    if (line.empty() || base::TrimWhitespaceASCII(line, base::TRIM_ALL).empty()) {
      continue;
    }

    std::vector<std::string> fields = ParseCsvLineInternal(line, delimiter);
    rows.push_back(std::move(fields));
  }

  if (rows.empty()) {
    return base::Value();
  }

  // Determine if first row is headers
  bool has_headers = false;
  if (rows.size() > 1) {
    // Simple heuristic: if all fields in first row are non-numeric text,
    // and subsequent rows have numeric data, it's likely headers
    bool first_row_all_text = true;
    bool has_numeric_in_data = false;

    for (const auto& field : rows[0]) {
      double value;
      if (base::StringToDouble(field, &value)) {
        first_row_all_text = false;
        break;
      }
    }

    // Check if there are numbers in subsequent rows
    for (size_t i = 1; i < std::min(rows.size(), size_t{5}); ++i) {
      for (const auto& field : rows[i]) {
        double value;
        if (base::StringToDouble(field, &value)) {
          has_numeric_in_data = true;
          break;
        }
      }
      if (has_numeric_in_data) break;
    }

    has_headers = first_row_all_text && has_numeric_in_data;
  }

  // Build result
  std::vector<std::string> headers;
  size_t data_start_row = 0;

  if (has_headers && !rows.empty()) {
    headers = rows[0];
    data_start_row = 1;
  } else if (!rows.empty()) {
    // Generate default headers
    for (size_t i = 0; i < rows[0].size(); ++i) {
      headers.push_back(base::StringPrintf("column_%zu", i));
    }
  }

  // Convert rows to array of objects
  base::Value::List row_data;
  for (size_t i = data_start_row; i < rows.size(); ++i) {
    base::Value::Dict row_dict;
    for (size_t j = 0; j < rows[i].size() && j < headers.size(); ++j) {
      row_dict.Set(headers[j], rows[i][j]);
    }
    row_data.Append(std::move(row_dict));
  }

  // Build final structure
  base::Value::Dict result;
  base::Value::List header_list;
  for (const auto& h : headers) {
    header_list.Append(h);
  }
  result.Set("headers", std::move(header_list));
  result.Set("rows", std::move(row_data));
  result.Set("has_headers", has_headers);

  return base::Value(std::move(result));
}

char CsvTableExtractor::DetectDelimiter(const std::string& text) {
  // Count occurrences of potential delimiters in first few lines
  std::vector<std::string> sample_lines = base::SplitString(
      text, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (sample_lines.empty()) {
    return ',';  // Default to comma
  }

  // Only check first 5 lines
  size_t lines_to_check = std::min(sample_lines.size(), size_t{5});

  std::map<char, int> delimiter_counts;
  delimiter_counts[','] = 0;
  delimiter_counts['\t'] = 0;
  delimiter_counts[';'] = 0;
  delimiter_counts['|'] = 0;

  for (size_t i = 0; i < lines_to_check; ++i) {
    const std::string& line = sample_lines[i];
    bool in_quotes = false;

    for (char c : line) {
      if (c == '"') {
        in_quotes = !in_quotes;
      } else if (!in_quotes && IsPotentialDelimiter(c)) {
        delimiter_counts[c]++;
      }
    }
  }

  // Find most common delimiter
  char best_delimiter = ',';
  int max_count = 0;

  for (const auto& [delim, count] : delimiter_counts) {
    if (count > max_count) {
      max_count = count;
      best_delimiter = delim;
    }
  }

  return max_count > 0 ? best_delimiter : ',';
}

bool CsvTableExtractor::HasHeaders(
    const std::vector<std::vector<std::string>>& rows) {
  if (rows.size() < 2) {
    return false;
  }

  // Check if first row is all text and subsequent rows have numbers
  bool first_row_all_text = true;
  for (const auto& field : rows[0]) {
    double value;
    if (base::StringToDouble(field, &value)) {
      first_row_all_text = false;
      break;
    }
  }

  if (!first_row_all_text) {
    return false;
  }

  // Check for numeric data in subsequent rows
  for (size_t i = 1; i < std::min(rows.size(), size_t{5}); ++i) {
    for (const auto& field : rows[i]) {
      double value;
      if (base::StringToDouble(field, &value)) {
        return true;
      }
    }
  }

  return false;
}

std::vector<std::string> CsvTableExtractor::ParseCsvLine(
    const std::string& line,
    char delimiter) {
  return ParseCsvLineInternal(line, delimiter);
}

}  // namespace datasipper
