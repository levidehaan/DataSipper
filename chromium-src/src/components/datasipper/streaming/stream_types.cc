// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/streaming/stream_types.h"

#include <cmath>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace datasipper {

// TimingPattern

TimingPattern::TimingPattern() = default;
TimingPattern::TimingPattern(const TimingPattern& other) = default;
TimingPattern& TimingPattern::operator=(const TimingPattern& other) = default;
TimingPattern::~TimingPattern() = default;

// StreamDefinition

StreamDefinition::StreamDefinition() = default;
StreamDefinition::StreamDefinition(const StreamDefinition& other) = default;
StreamDefinition& StreamDefinition::operator=(const StreamDefinition& other) =
    default;
StreamDefinition::~StreamDefinition() = default;

base::Value StreamDefinition::ToValue() const {
  base::Value::Dict dict;

  dict.Set("stream_id", stream_id);
  dict.Set("label", label);
  dict.Set("type", static_cast<int>(type));
  dict.Set("source_pattern", source_pattern);
  dict.Set("url", url);

  // Timing
  dict.Set("avg_interval_ms", timing.avg_interval_ms);
  dict.Set("std_deviation", timing.std_deviation);
  dict.Set("is_regular", timing.is_regular);
  dict.Set("pattern_type", static_cast<int>(timing.type));

  // Parameters
  base::Value::Dict params_dict;
  for (const auto& param : parameters) {
    params_dict.Set(param.first, param.second);
  }
  dict.Set("parameters", std::move(params_dict));

  // Classification
  dict.Set("data_type", static_cast<int>(data_type));
  dict.Set("schema", schema);

  // Lifecycle
  dict.Set("first_seen", static_cast<double>(first_seen.InMillisecondsSinceUnixEpoch()));
  dict.Set("last_seen", static_cast<double>(last_seen.InMillisecondsSinceUnixEpoch()));
  dict.Set("event_count", static_cast<double>(event_count));
  dict.Set("is_active", is_active);
  dict.Set("is_subscribed", is_subscribed);

  return base::Value(std::move(dict));
}

StreamDefinition StreamDefinition::FromValue(const base::Value& value) {
  StreamDefinition def;

  if (!value.is_dict()) {
    LOG(ERROR) << "StreamDefinition::FromValue: expected dict";
    return def;
  }

  const base::Value::Dict& dict = value.GetDict();

  const std::string* stream_id = dict.FindString("stream_id");
  if (stream_id) {
    def.stream_id = *stream_id;
  }

  const std::string* label = dict.FindString("label");
  if (label) {
    def.label = *label;
  }

  std::optional<int> type = dict.FindInt("type");
  if (type) {
    def.type = static_cast<StreamType>(*type);
  }

  const std::string* source_pattern = dict.FindString("source_pattern");
  if (source_pattern) {
    def.source_pattern = *source_pattern;
  }

  const std::string* url = dict.FindString("url");
  if (url) {
    def.url = *url;
  }

  // Timing
  std::optional<double> avg_interval_ms = dict.FindDouble("avg_interval_ms");
  if (avg_interval_ms) {
    def.timing.avg_interval_ms = *avg_interval_ms;
  }

  std::optional<double> std_deviation = dict.FindDouble("std_deviation");
  if (std_deviation) {
    def.timing.std_deviation = *std_deviation;
  }

  std::optional<bool> is_regular = dict.FindBool("is_regular");
  if (is_regular) {
    def.timing.is_regular = *is_regular;
  }

  std::optional<int> pattern_type = dict.FindInt("pattern_type");
  if (pattern_type) {
    def.timing.type = static_cast<PatternType>(*pattern_type);
  }

  // Parameters
  const base::Value::Dict* params = dict.FindDict("parameters");
  if (params) {
    for (const auto [key, val] : *params) {
      if (val.is_string()) {
        def.parameters[key] = val.GetString();
      }
    }
  }

  // Classification
  std::optional<int> data_type = dict.FindInt("data_type");
  if (data_type) {
    def.data_type = static_cast<DataType>(*data_type);
  }

  const std::string* schema = dict.FindString("schema");
  if (schema) {
    def.schema = *schema;
  }

  // Lifecycle
  std::optional<double> first_seen = dict.FindDouble("first_seen");
  if (first_seen) {
    def.first_seen = base::Time::FromMillisecondsSinceUnixEpoch(
        static_cast<int64_t>(*first_seen));
  }

  std::optional<double> last_seen = dict.FindDouble("last_seen");
  if (last_seen) {
    def.last_seen = base::Time::FromMillisecondsSinceUnixEpoch(
        static_cast<int64_t>(*last_seen));
  }

  std::optional<double> event_count = dict.FindDouble("event_count");
  if (event_count) {
    def.event_count = static_cast<int64_t>(*event_count);
  }

  std::optional<bool> is_active = dict.FindBool("is_active");
  if (is_active) {
    def.is_active = *is_active;
  }

  std::optional<bool> is_subscribed = dict.FindBool("is_subscribed");
  if (is_subscribed) {
    def.is_subscribed = *is_subscribed;
  }

  return def;
}

// StreamDataEvent

StreamDataEvent::StreamDataEvent() = default;
StreamDataEvent::StreamDataEvent(const StreamDataEvent& other) = default;
StreamDataEvent& StreamDataEvent::operator=(const StreamDataEvent& other) =
    default;
StreamDataEvent::~StreamDataEvent() = default;

base::Value StreamDataEvent::ToValue() const {
  base::Value::Dict dict;

  dict.Set("stream_id", stream_id);
  dict.Set("timestamp", static_cast<double>(timestamp.InMillisecondsSinceUnixEpoch()));
  dict.Set("data", data);

  base::Value::Dict metadata_dict;
  for (const auto& [key, val] : metadata) {
    metadata_dict.Set(key, val);
  }
  dict.Set("metadata", std::move(metadata_dict));

  return base::Value(std::move(dict));
}

StreamDataEvent StreamDataEvent::FromValue(const base::Value& value) {
  StreamDataEvent event;

  if (!value.is_dict()) {
    LOG(ERROR) << "StreamDataEvent::FromValue: expected dict";
    return event;
  }

  const base::Value::Dict& dict = value.GetDict();

  const std::string* stream_id = dict.FindString("stream_id");
  if (stream_id) {
    event.stream_id = *stream_id;
  }

  std::optional<double> timestamp = dict.FindDouble("timestamp");
  if (timestamp) {
    event.timestamp = base::Time::FromMillisecondsSinceUnixEpoch(
        static_cast<int64_t>(*timestamp));
  }

  const std::string* data = dict.FindString("data");
  if (data) {
    event.data = *data;
  }

  const base::Value::Dict* metadata = dict.FindDict("metadata");
  if (metadata) {
    for (const auto [key, val] : *metadata) {
      if (val.is_string()) {
        event.metadata[key] = val.GetString();
      }
    }
  }

  return event;
}

// ExportConfig

ExportConfig::ExportConfig() = default;
ExportConfig::~ExportConfig() = default;

// StreamSubscription

StreamSubscription::StreamSubscription() = default;
StreamSubscription::~StreamSubscription() = default;

}  // namespace datasipper
