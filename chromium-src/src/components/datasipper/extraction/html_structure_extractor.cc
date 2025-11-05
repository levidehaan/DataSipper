// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/extraction/html_structure_extractor.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace datasipper {

namespace {

// Simple HTML tag extraction helper
std::vector<std::string> ExtractTagContent(const std::string& html,
                                           const std::string& tag_name) {
  std::vector<std::string> results;
  std::string open_tag = "<" + tag_name;
  std::string close_tag = "</" + tag_name + ">";

  size_t pos = 0;
  while (true) {
    size_t start = html.find(open_tag, pos);
    if (start == std::string::npos) {
      break;
    }

    // Find the end of the opening tag
    size_t tag_end = html.find(">", start);
    if (tag_end == std::string::npos) {
      break;
    }

    // Find the closing tag
    size_t close_start = html.find(close_tag, tag_end);
    if (close_start == std::string::npos) {
      break;
    }

    std::string content = html.substr(tag_end + 1, close_start - tag_end - 1);
    results.push_back(content);

    pos = close_start + close_tag.length();
  }

  return results;
}

// Strip HTML tags from text
std::string StripTags(const std::string& html) {
  std::string result;
  bool in_tag = false;

  for (char c : html) {
    if (c == '<') {
      in_tag = true;
    } else if (c == '>') {
      in_tag = false;
    } else if (!in_tag) {
      result += c;
    }
  }

  base::TrimWhitespaceASCII(result, base::TRIM_ALL, &result);
  return result;
}

}  // namespace

HtmlStructureExtractor::HtmlStructureExtractor() = default;
HtmlStructureExtractor::~HtmlStructureExtractor() = default;

std::vector<ExtractedDataSource> HtmlStructureExtractor::ExtractTables(
    const std::string& html) {
  std::vector<ExtractedDataSource> results;

  // Extract all <table> elements
  std::vector<std::string> tables = ExtractTagContent(html, "table");

  for (size_t i = 0; i < tables.size(); ++i) {
    const std::string& table_html = tables[i];

    // Parse table into structured data
    base::Value parsed_table = ParseTable(table_html);
    if (parsed_table.is_none()) {
      continue;
    }

    ExtractedDataSource source;
    source.type = DataSourceType::HTML_TABLE;
    source.selector_path = base::StringPrintf("table:nth(%zu)", i);
    source.label = base::StringPrintf("Table %zu", i + 1);
    source.raw_content = table_html;
    source.parsed_data = std::move(parsed_table);
    source.confidence_score = HasTableHeaders(table_html) ? 1.0f : 0.7f;

    // Count rows and columns
    if (source.parsed_data.is_dict()) {
      const base::Value::Dict& dict = source.parsed_data.GetDict();
      if (const base::Value::List* rows = dict.FindList("rows")) {
        source.metadata["row_count"] = base::NumberToString(rows->size());
      }
      if (const base::Value::List* headers = dict.FindList("headers")) {
        source.metadata["column_count"] = base::NumberToString(headers->size());
      }
    }

    results.push_back(std::move(source));
  }

  return results;
}

std::vector<ExtractedDataSource> HtmlStructureExtractor::ExtractLists(
    const std::string& html) {
  std::vector<ExtractedDataSource> results;

  // Extract both <ul> and <ol> lists
  std::vector<std::string> ul_lists = ExtractTagContent(html, "ul");
  std::vector<std::string> ol_lists = ExtractTagContent(html, "ol");

  auto process_lists = [&](const std::vector<std::string>& lists,
                           const std::string& list_type) {
    for (size_t i = 0; i < lists.size(); ++i) {
      const std::string& list_html = lists[i];

      // Extract list items
      std::vector<std::string> items = ExtractTagContent(list_html, "li");
      if (items.empty()) {
        continue;
      }

      // Convert to base::Value
      base::Value::List list_data;
      for (const auto& item : items) {
        std::string text = StripTags(item);
        if (!text.empty()) {
          list_data.Append(text);
        }
      }

      if (list_data.empty()) {
        continue;
      }

      ExtractedDataSource source;
      source.type = DataSourceType::HTML_TABLE;  // Reuse table type for lists
      source.selector_path = base::StringPrintf("%s:nth(%zu)",
                                                list_type.c_str(), i);
      source.label = base::StringPrintf("%s List %zu",
                                        list_type == "ul" ? "Unordered" : "Ordered",
                                        i + 1);
      source.raw_content = list_html;

      base::Value::Dict dict;
      dict.Set("type", list_type);
      dict.Set("items", std::move(list_data));
      source.parsed_data = base::Value(std::move(dict));

      source.confidence_score = 0.8f;
      source.metadata["item_count"] = base::NumberToString(items.size());

      results.push_back(std::move(source));
    }
  };

  process_lists(ul_lists, "ul");
  process_lists(ol_lists, "ol");

  return results;
}

std::vector<ExtractedDataSource> HtmlStructureExtractor::ExtractForms(
    const std::string& html) {
  std::vector<ExtractedDataSource> results;

  // Extract all <form> elements
  std::vector<std::string> forms = ExtractTagContent(html, "form");

  for (size_t i = 0; i < forms.size(); ++i) {
    const std::string& form_html = forms[i];

    // Extract input fields
    base::Value::List fields;

    // Simple pattern matching for input elements
    size_t pos = 0;
    while (true) {
      size_t input_start = form_html.find("<input", pos);
      if (input_start == std::string::npos) {
        break;
      }

      size_t input_end = form_html.find(">", input_start);
      if (input_end == std::string::npos) {
        break;
      }

      std::string input_tag = form_html.substr(input_start,
                                               input_end - input_start + 1);

      // Extract attributes
      base::Value::Dict field_info;

      // Extract name attribute
      size_t name_pos = input_tag.find("name=");
      if (name_pos != std::string::npos) {
        size_t name_start = input_tag.find_first_of("\"'", name_pos) + 1;
        size_t name_end = input_tag.find_first_of("\"'", name_start);
        if (name_end != std::string::npos) {
          std::string name = input_tag.substr(name_start, name_end - name_start);
          field_info.Set("name", name);
        }
      }

      // Extract type attribute
      size_t type_pos = input_tag.find("type=");
      if (type_pos != std::string::npos) {
        size_t type_start = input_tag.find_first_of("\"'", type_pos) + 1;
        size_t type_end = input_tag.find_first_of("\"'", type_start);
        if (type_end != std::string::npos) {
          std::string type = input_tag.substr(type_start, type_end - type_start);
          field_info.Set("type", type);
        }
      } else {
        field_info.Set("type", "text");  // Default type
      }

      if (!field_info.empty()) {
        fields.Append(std::move(field_info));
      }

      pos = input_end + 1;
    }

    if (fields.empty()) {
      continue;
    }

    ExtractedDataSource source;
    source.type = DataSourceType::FORM_DATA;
    source.selector_path = base::StringPrintf("form:nth(%zu)", i);
    source.label = base::StringPrintf("Form %zu", i + 1);
    source.raw_content = form_html;

    base::Value::Dict form_data;
    form_data.Set("fields", std::move(fields));
    source.parsed_data = base::Value(std::move(form_data));

    source.confidence_score = 0.9f;

    results.push_back(std::move(source));
  }

  return results;
}

base::Value HtmlStructureExtractor::ParseTable(const std::string& table_html) {
  // Extract headers from <thead> or first <tr>
  std::vector<std::string> headers;
  bool has_thead = table_html.find("<thead") != std::string::npos;

  std::string header_section = table_html;
  if (has_thead) {
    std::vector<std::string> theads = ExtractTagContent(table_html, "thead");
    if (!theads.empty()) {
      header_section = theads[0];
    }
  }

  // Extract <th> elements for headers
  std::vector<std::string> th_elements = ExtractTagContent(header_section, "th");
  for (const auto& th : th_elements) {
    std::string text = StripTags(th);
    if (!text.empty()) {
      headers.push_back(text);
    }
  }

  // If no <th> found, try first row of <td>
  if (headers.empty()) {
    std::vector<std::string> rows = ExtractTagContent(table_html, "tr");
    if (!rows.empty()) {
      std::vector<std::string> cells = ExtractTagContent(rows[0], "td");
      for (const auto& cell : cells) {
        std::string text = StripTags(cell);
        headers.push_back(text.empty() ? "Column" : text);
      }
    }
  }

  // Extract rows from <tbody> or all <tr> elements
  std::vector<std::string> tbody_list = ExtractTagContent(table_html, "tbody");
  std::string body_section = tbody_list.empty() ? table_html : tbody_list[0];

  std::vector<std::string> rows = ExtractTagContent(body_section, "tr");

  // Parse rows
  base::Value::List row_data;
  size_t start_row = has_thead ? 0 : 1;  // Skip first row if it was headers

  for (size_t i = start_row; i < rows.size(); ++i) {
    std::vector<std::string> cells = ExtractTagContent(rows[i], "td");
    if (cells.empty()) {
      continue;
    }

    base::Value::Dict row_dict;
    for (size_t j = 0; j < cells.size(); ++j) {
      std::string text = StripTags(cells[j]);
      std::string header = j < headers.size()
          ? headers[j]
          : base::StringPrintf("column_%zu", j);
      row_dict.Set(header, text);
    }

    row_data.Append(std::move(row_dict));
  }

  if (row_data.empty()) {
    return base::Value();
  }

  // Build result
  base::Value::Dict result;
  base::Value::List header_list;
  for (const auto& h : headers) {
    header_list.Append(h);
  }
  result.Set("headers", std::move(header_list));
  result.Set("rows", std::move(row_data));

  return base::Value(std::move(result));
}

bool HtmlStructureExtractor::HasTableHeaders(const std::string& table_html) {
  return table_html.find("<thead") != std::string::npos ||
         table_html.find("<th") != std::string::npos;
}

}  // namespace datasipper
