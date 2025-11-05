// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/streaming/network_stream_analyzer.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace datasipper {

// PollingPattern implementation
PollingPattern::PollingPattern() = default;
PollingPattern::~PollingPattern() = default;
PollingPattern::PollingPattern(const PollingPattern&) = default;
PollingPattern& PollingPattern::operator=(const PollingPattern&) = default;

// WebSocketStream implementation
WebSocketStream::WebSocketStream() = default;
WebSocketStream::~WebSocketStream() = default;
WebSocketStream::WebSocketStream(const WebSocketStream&) = default;
WebSocketStream& WebSocketStream::operator=(const WebSocketStream&) = default;

// NetworkStreamAnalyzer implementation

NetworkStreamAnalyzer::NetworkStreamAnalyzer() {
  DVLOG(1) << "NetworkStreamAnalyzer created";
}

NetworkStreamAnalyzer::~NetworkStreamAnalyzer() {
  DisableAnalysis();
}

void NetworkStreamAnalyzer::EnableAnalysis(StreamDetectedCallback callback) {
  analyzing_enabled_ = true;
  stream_detected_callback_ = std::move(callback);

  LOG(INFO) << "Network stream analysis enabled";

  // Start periodic checking for new streams
  check_timer_.Start(FROM_HERE, base::Seconds(kCheckIntervalSeconds), this,
                     &NetworkStreamAnalyzer::CheckForNewStreams);
}

void NetworkStreamAnalyzer::DisableAnalysis() {
  analyzing_enabled_ = false;
  stream_detected_callback_.Reset();
  check_timer_.Stop();
  polling_patterns_.clear();
  websocket_streams_.clear();

  LOG(INFO) << "Network stream analysis disabled";
}

void NetworkStreamAnalyzer::ProcessNetworkEvent(const NetworkEvent& event) {
  if (!analyzing_enabled_) {
    return;
  }

  switch (event.type) {
    case EventType::HTTP_REQUEST:
      ProcessHttpRequest(event);
      break;
    case EventType::HTTP_RESPONSE:
      ProcessHttpResponse(event);
      break;
    case EventType::WEBSOCKET_CONNECT:
      ProcessWebSocketConnect(event);
      break;
    case EventType::WEBSOCKET_MESSAGE_SENT:
    case EventType::WEBSOCKET_MESSAGE_RECEIVED:
      ProcessWebSocketMessage(event);
      break;
    case EventType::WEBSOCKET_DISCONNECT:
      ProcessWebSocketDisconnect(event);
      break;
  }
}

std::vector<StreamDefinition> NetworkStreamAnalyzer::GetActiveStreams() const {
  std::vector<StreamDefinition> streams;

  // Add polling streams
  for (const auto& [pattern_key, pattern] : polling_patterns_) {
    if (pattern.reported && IsPollingStream(pattern)) {
      StreamDefinition stream;
      stream.stream_id =
          GenerateStreamId("http_polling", pattern.url_pattern);
      stream.label = pattern.url.spec();
      stream.type = StreamType::NETWORK_PATTERN;
      stream.source_pattern = pattern.url_pattern;
      stream.url = pattern.url.spec();
      stream.first_seen = pattern.first_seen;
      stream.last_seen = pattern.last_seen;
      stream.is_active = true;
      stream.event_count = pattern.request_count;
      stream.data_type = DataType::JSON;  // Assume JSON for now
      stream.timing.avg_interval_ms = pattern.avg_interval_ms;
      stream.timing.std_deviation = pattern.std_deviation;
      stream.timing.is_regular = pattern.is_regular;
      stream.timing.type = pattern.is_regular ? PatternType::POLLING
                                               : PatternType::EVENT_DRIVEN;
      streams.push_back(stream);
    }
  }

  // Add WebSocket streams
  for (const auto& [conn_id, ws_stream] : websocket_streams_) {
    if (ws_stream.reported && ws_stream.is_active) {
      StreamDefinition stream;
      stream.stream_id = GenerateStreamId("websocket", conn_id);
      stream.label = ws_stream.url.spec();
      stream.type = StreamType::WEBSOCKET_STREAM;
      stream.source_pattern = ws_stream.url.spec();
      stream.url = ws_stream.url.spec();
      stream.first_seen = ws_stream.connected_at;
      stream.last_seen = ws_stream.last_message_at;
      stream.is_active = ws_stream.is_active;
      stream.event_count =
          ws_stream.messages_sent + ws_stream.messages_received;
      stream.data_type = DataType::JSON;  // Assume JSON for now
      streams.push_back(stream);
    }
  }

  return streams;
}

void NetworkStreamAnalyzer::ProcessHttpRequest(const NetworkEvent& event) {
  std::string pattern_key = NormalizeUrl(event.url);
  UpdatePollingPattern(pattern_key, event.url);
}

void NetworkStreamAnalyzer::ProcessHttpResponse(const NetworkEvent& event) {
  // Could analyze response patterns here
  // For now, we focus on request patterns
}

void NetworkStreamAnalyzer::ProcessWebSocketConnect(const NetworkEvent& event) {
  WebSocketStream stream;
  stream.connection_id = event.connection_id;
  stream.url = event.url;
  stream.connected_at = event.timestamp;
  stream.last_message_at = event.timestamp;
  stream.is_active = true;
  stream.reported = false;

  websocket_streams_[event.connection_id] = stream;

  LOG(INFO) << "WebSocket connection detected: " << event.url.spec();
}

void NetworkStreamAnalyzer::ProcessWebSocketMessage(const NetworkEvent& event) {
  auto it = websocket_streams_.find(event.connection_id);
  if (it == websocket_streams_.end()) {
    return;
  }

  WebSocketStream& stream = it->second;
  stream.last_message_at = event.timestamp;

  if (event.type == EventType::WEBSOCKET_MESSAGE_SENT) {
    stream.messages_sent++;
  } else {
    stream.messages_received++;
  }

  // Report if not yet reported and has enough activity
  if (!stream.reported && stream.messages_received >= 3) {
    ReportWebSocketStream(stream);
    stream.reported = true;
  }
}

void NetworkStreamAnalyzer::ProcessWebSocketDisconnect(
    const NetworkEvent& event) {
  auto it = websocket_streams_.find(event.connection_id);
  if (it != websocket_streams_.end()) {
    it->second.is_active = false;
    LOG(INFO) << "WebSocket disconnected: " << event.url.spec();
  }
}

void NetworkStreamAnalyzer::UpdatePollingPattern(const std::string& pattern_key,
                                                   const GURL& url) {
  auto it = polling_patterns_.find(pattern_key);

  if (it == polling_patterns_.end()) {
    // New pattern
    PollingPattern pattern;
    pattern.url = url;
    pattern.url_pattern = pattern_key;
    pattern.request_times.push_back(base::Time::Now());
    pattern.request_count = 1;
    pattern.first_seen = base::Time::Now();
    pattern.last_seen = base::Time::Now();
    pattern.reported = false;

    polling_patterns_[pattern_key] = pattern;
  } else {
    // Existing pattern
    PollingPattern& pattern = it->second;
    pattern.request_times.push_back(base::Time::Now());
    pattern.request_count++;
    pattern.last_seen = base::Time::Now();

    // Keep only recent timestamps
    if (pattern.request_times.size() >
        static_cast<size_t>(kMaxTrackedTimestamps)) {
      pattern.request_times.erase(pattern.request_times.begin());
    }

    // Analyze timing
    AnalyzePollingTiming(pattern);
  }
}

void NetworkStreamAnalyzer::AnalyzePollingTiming(PollingPattern& pattern) {
  if (pattern.request_times.size() < 2) {
    return;
  }

  // Calculate intervals
  std::vector<double> intervals;
  for (size_t i = 1; i < pattern.request_times.size(); i++) {
    base::TimeDelta delta =
        pattern.request_times[i] - pattern.request_times[i - 1];
    intervals.push_back(delta.InMillisecondsF());
  }

  // Calculate average
  double sum = 0.0;
  for (double interval : intervals) {
    sum += interval;
  }
  pattern.avg_interval_ms = sum / intervals.size();

  // Calculate standard deviation
  double sq_sum = 0.0;
  for (double interval : intervals) {
    sq_sum += std::pow(interval - pattern.avg_interval_ms, 2);
  }
  pattern.std_deviation = std::sqrt(sq_sum / intervals.size());

  // Calculate coefficient of variation
  pattern.coefficient_of_variation =
      pattern.avg_interval_ms > 0.0
          ? pattern.std_deviation / pattern.avg_interval_ms
          : 0.0;

  // Determine if regular
  pattern.is_regular =
      pattern.coefficient_of_variation < kRegularityThreshold;
}

bool NetworkStreamAnalyzer::IsPollingStream(
    const PollingPattern& pattern) const {
  if (pattern.request_count < kMinPollingRequests) {
    return false;
  }

  if (pattern.avg_interval_ms < kMinRequestIntervalMs ||
      pattern.avg_interval_ms > kMaxRequestIntervalMs) {
    return false;
  }

  return true;
}

void NetworkStreamAnalyzer::CheckForNewStreams() {
  // Check polling patterns
  for (auto& [pattern_key, pattern] : polling_patterns_) {
    if (!pattern.reported && IsPollingStream(pattern)) {
      ReportPollingStream(pattern);
      pattern.reported = true;
    }
  }

  // WebSocket streams are reported immediately when active
}

void NetworkStreamAnalyzer::ReportPollingStream(const PollingPattern& pattern) {
  StreamDefinition stream;
  stream.stream_id = GenerateStreamId("http_polling", pattern.url_pattern);
  stream.label = pattern.url.spec();
  stream.type = StreamType::NETWORK_PATTERN;
  stream.source_pattern = pattern.url_pattern;
  stream.url = pattern.url.spec();
  stream.first_seen = pattern.first_seen;
  stream.last_seen = pattern.last_seen;
  stream.is_active = true;
  stream.event_count = pattern.request_count;
  stream.data_type = DataType::JSON;  // Could analyze response content
  stream.timing.avg_interval_ms = pattern.avg_interval_ms;
  stream.timing.std_deviation = pattern.std_deviation;
  stream.timing.is_regular = pattern.is_regular;
  stream.timing.type =
      pattern.is_regular ? PatternType::POLLING : PatternType::EVENT_DRIVEN;

  LOG(INFO) << "HTTP polling stream detected: " << pattern.url.spec()
            << " (interval: " << pattern.avg_interval_ms << "ms"
            << ", requests: " << pattern.request_count << ")";

  if (stream_detected_callback_) {
    stream_detected_callback_.Run(stream);
  }
}

void NetworkStreamAnalyzer::ReportWebSocketStream(
    const WebSocketStream& ws_stream) {
  StreamDefinition stream;
  stream.stream_id = GenerateStreamId("websocket", ws_stream.connection_id);
  stream.label = ws_stream.url.spec();
  stream.type = StreamType::WEBSOCKET_STREAM;
  stream.source_pattern = ws_stream.url.spec();
  stream.url = ws_stream.url.spec();
  stream.first_seen = ws_stream.connected_at;
  stream.last_seen = ws_stream.last_message_at;
  stream.is_active = ws_stream.is_active;
  stream.event_count =
      ws_stream.messages_sent + ws_stream.messages_received;
  stream.data_type = DataType::JSON;  // Could analyze message content

  LOG(INFO) << "WebSocket stream detected: " << ws_stream.url.spec()
            << " (messages: " << stream.event_count << ")";

  if (stream_detected_callback_) {
    stream_detected_callback_.Run(stream);
  }
}

std::string NetworkStreamAnalyzer::NormalizeUrl(const GURL& url) const {
  // Remove query parameters and fragment
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements).spec();
}

std::string NetworkStreamAnalyzer::GenerateStreamId(
    const std::string& prefix,
    const std::string& identifier) const {
  return base::StrCat({prefix, "_", identifier});
}

}  // namespace datasipper
