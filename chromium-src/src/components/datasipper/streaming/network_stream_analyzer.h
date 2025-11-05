// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_STREAMING_NETWORK_STREAM_ANALYZER_H_
#define COMPONENTS_DATASIPPER_STREAMING_NETWORK_STREAM_ANALYZER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/datasipper/common/network_event.h"
#include "components/datasipper/streaming/stream_types.h"
#include "url/gurl.h"

namespace datasipper {

// Tracks HTTP polling patterns
struct PollingPattern {
  GURL url;
  std::string url_pattern;  // Normalized URL pattern (without query params)
  std::vector<base::Time> request_times;
  int request_count = 0;
  base::Time first_seen;
  base::Time last_seen;
  bool reported = false;

  // Timing statistics
  double avg_interval_ms = 0.0;
  double std_deviation = 0.0;
  double coefficient_of_variation = 0.0;
  bool is_regular = false;

  PollingPattern();
  ~PollingPattern();
  PollingPattern(const PollingPattern&);
  PollingPattern& operator=(const PollingPattern&);
};

// Tracks WebSocket connections
struct WebSocketStream {
  std::string connection_id;
  GURL url;
  base::Time connected_at;
  base::Time last_message_at;
  int messages_sent = 0;
  int messages_received = 0;
  bool is_active = true;
  bool reported = false;

  WebSocketStream();
  ~WebSocketStream();
  WebSocketStream(const WebSocketStream&);
  WebSocketStream& operator=(const WebSocketStream&);
};

// Analyzes network traffic to detect streaming patterns
class NetworkStreamAnalyzer {
 public:
  using StreamDetectedCallback =
      base::RepeatingCallback<void(const StreamDefinition&)>;

  NetworkStreamAnalyzer();
  ~NetworkStreamAnalyzer();

  NetworkStreamAnalyzer(const NetworkStreamAnalyzer&) = delete;
  NetworkStreamAnalyzer& operator=(const NetworkStreamAnalyzer&) = delete;

  // Enable/disable analysis
  void EnableAnalysis(StreamDetectedCallback callback);
  void DisableAnalysis();
  bool IsAnalyzing() const { return analyzing_enabled_; }

  // Process network events
  void ProcessNetworkEvent(const NetworkEvent& event);

  // Get currently detected streams
  std::vector<StreamDefinition> GetActiveStreams() const;

 private:
  // Event handlers
  void ProcessHttpRequest(const NetworkEvent& event);
  void ProcessHttpResponse(const NetworkEvent& event);
  void ProcessWebSocketConnect(const NetworkEvent& event);
  void ProcessWebSocketMessage(const NetworkEvent& event);
  void ProcessWebSocketDisconnect(const NetworkEvent& event);

  // Pattern analysis
  void UpdatePollingPattern(const std::string& pattern_key, const GURL& url);
  void AnalyzePollingTiming(PollingPattern& pattern);
  bool IsPollingStream(const PollingPattern& pattern) const;

  // Stream reporting
  void CheckForNewStreams();
  void ReportPollingStream(const PollingPattern& pattern);
  void ReportWebSocketStream(const WebSocketStream& stream);

  // Utility functions
  std::string NormalizeUrl(const GURL& url) const;
  std::string GenerateStreamId(const std::string& prefix,
                                const std::string& identifier) const;

  // State
  bool analyzing_enabled_ = false;
  StreamDetectedCallback stream_detected_callback_;

  // Tracking structures
  std::map<std::string, PollingPattern> polling_patterns_;  // pattern -> data
  std::map<std::string, WebSocketStream> websocket_streams_;  // connection_id -> stream

  // Periodic check timer
  base::RepeatingTimer check_timer_;

  // Configuration
  static constexpr int kMinPollingRequests = 3;
  static constexpr int kMinRequestIntervalMs = 100;
  static constexpr int kMaxRequestIntervalMs = 300000;  // 5 minutes
  static constexpr double kRegularityThreshold = 0.2;
  static constexpr int kMaxTrackedTimestamps = 50;
  static constexpr int kCheckIntervalSeconds = 10;

  base::WeakPtrFactory<NetworkStreamAnalyzer> weak_factory_{this};
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_STREAMING_NETWORK_STREAM_ANALYZER_H_
