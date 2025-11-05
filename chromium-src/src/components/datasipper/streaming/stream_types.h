// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_STREAMING_STREAM_TYPES_H_
#define COMPONENTS_DATASIPPER_STREAMING_STREAM_TYPES_H_

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"

namespace datasipper {

// Types of data streams
enum class StreamType {
  DOM_MUTATION = 0,    // Elements that change regularly
  NETWORK_PATTERN = 1, // Repeated network requests
  WEBSOCKET_STREAM = 2 // WebSocket message streams
};

// Data type classification
enum class DataType {
  UNKNOWN = 0,
  NUMERIC = 1,    // Numbers (int, float, currency)
  TEXT = 2,       // Plain text
  JSON = 3,       // JSON object/array
  XML = 4,        // XML data
  BINARY = 5      // Binary data
};

// Pattern type for timing analysis
enum class PatternType {
  UNKNOWN = 0,
  POLLING = 1,       // Regular intervals (e.g., every 5s)
  EVENT_DRIVEN = 2,  // Irregular, triggered by events
  BURST = 3          // Clustered requests
};

// Timing pattern analysis
struct TimingPattern {
  TimingPattern();
  TimingPattern(const TimingPattern& other);
  TimingPattern& operator=(const TimingPattern& other);
  ~TimingPattern();

  double avg_interval_ms = 0.0;
  double std_deviation = 0.0;
  bool is_regular = false;      // Coefficient of variation < 0.2
  PatternType type = PatternType::UNKNOWN;

  // Timestamps of observed events
  std::vector<base::Time> event_timestamps;
};

// Stream definition
struct StreamDefinition {
  StreamDefinition();
  StreamDefinition(const StreamDefinition& other);
  StreamDefinition& operator=(const StreamDefinition& other);
  ~StreamDefinition();

  // Identification
  std::string stream_id;          // Unique identifier
  std::string label;              // Human-readable name
  StreamType type = StreamType::DOM_MUTATION;

  // Source
  std::string source_pattern;     // CSS selector or URL pattern
  std::string url;                // Page URL where stream was detected

  // Timing
  TimingPattern timing;

  // Parameters (for network streams)
  std::map<std::string, std::string> parameters;

  // Classification
  DataType data_type = DataType::UNKNOWN;
  std::string schema;             // Optional JSON schema

  // Lifecycle
  base::Time first_seen;
  base::Time last_seen;
  int64_t event_count = 0;
  bool is_active = false;

  // Subscription
  bool is_subscribed = false;

  // Convert to base::Value for serialization
  base::Value ToValue() const;

  // Create from base::Value
  static StreamDefinition FromValue(const base::Value& value);
};

// Stream data event
struct StreamDataEvent {
  StreamDataEvent();
  StreamDataEvent(const StreamDataEvent& other);
  StreamDataEvent& operator=(const StreamDataEvent& other);
  ~StreamDataEvent();

  std::string stream_id;
  base::Time timestamp;
  std::string data;               // JSON-encoded or raw data
  std::map<std::string, std::string> metadata;

  // Convert to base::Value
  base::Value ToValue() const;

  // Create from base::Value
  static StreamDataEvent FromValue(const base::Value& value);
};

// Export configuration
enum class ExportType {
  KAFKA = 0,
  WEBHOOK = 1,
  REDIS = 2,
  FILE = 3,
  WEBSOCKET = 4
};

struct ExportConfig {
  ExportConfig();
  ~ExportConfig();

  ExportType type = ExportType::FILE;
  std::map<std::string, std::string> config;  // Type-specific configuration

  // Data handling
  bool buffer_enabled = false;
  int buffer_size_mb = 10;
  bool include_metadata = true;
};

// Stream subscription
struct StreamSubscription {
  StreamSubscription();
  ~StreamSubscription();

  std::string subscription_id;
  std::string stream_id;
  ExportConfig export_config;

  // Lifecycle
  base::Time subscribed_at;
  int64_t events_processed = 0;
  bool is_active = false;
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_STREAMING_STREAM_TYPES_H_
