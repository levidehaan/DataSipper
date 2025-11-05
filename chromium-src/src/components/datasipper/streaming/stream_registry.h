// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_STREAMING_STREAM_REGISTRY_H_
#define COMPONENTS_DATASIPPER_STREAMING_STREAM_REGISTRY_H_

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/datasipper/streaming/stream_types.h"

namespace datasipper {

// Individual event from a stream
struct StreamEvent {
  std::string stream_id;
  base::Time timestamp;
  base::Value data;
  std::string event_type;

  StreamEvent();
  ~StreamEvent();
  StreamEvent(const StreamEvent&);
  StreamEvent& operator=(const StreamEvent&);
  StreamEvent(StreamEvent&&) noexcept;
  StreamEvent& operator=(StreamEvent&&) noexcept;
};

// Options for stream subscriptions
struct StreamSubscriptionOptions {
  std::optional<std::string> event_type_filter;
  std::optional<base::TimeDelta> min_interval;
  std::optional<int> max_events;
  std::optional<base::TimeDelta> max_duration;
  bool include_recent_events = false;
  int recent_event_count = 0;

  StreamSubscriptionOptions();
  ~StreamSubscriptionOptions();
};

// Subscriber callback type
using StreamEventCallback = base::RepeatingCallback<void(const StreamEvent&)>;

// Central registry for managing detected streams
class StreamRegistry {
 public:
  StreamRegistry();
  ~StreamRegistry();

  StreamRegistry(const StreamRegistry&) = delete;
  StreamRegistry& operator=(const StreamRegistry&) = delete;

  // Stream registration (called when new streams detected)
  std::string RegisterStream(const StreamDefinition& stream);
  void UpdateStream(const std::string& stream_id,
                    const StreamDefinition& updated_stream);
  void RemoveStream(const std::string& stream_id);

  // Stream queries
  std::optional<StreamDefinition> GetStream(
      const std::string& stream_id) const;
  std::vector<StreamDefinition> GetAllStreams() const;
  std::vector<StreamDefinition> GetActiveStreams() const;
  std::vector<StreamDefinition> GetStreamsByType(StreamType type) const;
  std::vector<std::string> GetStreamIdsByPattern(
      const std::string& pattern) const;

  // Stream state management
  void SetStreamActive(const std::string& stream_id, bool active);
  bool IsStreamActive(const std::string& stream_id) const;

  // Event publication
  void PublishEvent(const std::string& stream_id,
                    const base::Value& data,
                    const std::string& event_type = "update");

  // Subscription management
  std::string Subscribe(const std::string& stream_id,
                        StreamEventCallback callback,
                        const StreamSubscriptionOptions& options = {});
  void Unsubscribe(const std::string& subscription_id);
  int GetSubscriberCount(const std::string& stream_id) const;

  // Pattern matching for workflows
  std::vector<std::string> MatchUrlPattern(const std::string& url_pattern) const;
  std::vector<std::string> MatchSourcePattern(
      const std::string& source_pattern) const;

  // Statistics
  size_t GetStreamCount() const { return streams_.size(); }
  size_t GetActiveStreamCount() const;
  size_t GetTotalEventCount() const;

  // Recent events access (for backfill, debugging)
  std::vector<StreamEvent> GetRecentEvents(
      const std::string& stream_id,
      int count = 10) const;

 private:
  struct ManagedStream {
    StreamDefinition definition;
    base::Time registered_at;
    base::Time last_update;
    bool is_active = false;
    int event_count_total = 0;
    std::deque<StreamEvent> recent_events;
    static constexpr size_t kMaxRecentEvents = 100;

    ManagedStream();
    ~ManagedStream();
  };

  struct Subscriber {
    std::string subscriber_id;
    std::string stream_id;
    StreamEventCallback callback;
    StreamSubscriptionOptions options;
    base::Time subscribed_at;
    int events_received = 0;
    base::Time last_event_time;

    Subscriber();
    ~Subscriber();
  };

  // Deduplication helper
  std::optional<std::string> FindDuplicateStream(
      const StreamDefinition& stream) const;

  // Subscription helpers
  void NotifySubscribers(const std::string& stream_id,
                         const StreamEvent& event);
  bool ShouldDeliverEvent(const Subscriber& subscriber,
                          const StreamEvent& event) const;
  void CheckSubscriberLimits(const std::string& subscription_id);

  // Pattern indexing
  void IndexStreamPattern(const std::string& stream_id,
                          const StreamDefinition& stream);
  void RemoveStreamFromIndex(const std::string& stream_id);

  // Storage
  std::map<std::string, ManagedStream> streams_;
  std::map<std::string, Subscriber> subscribers_;

  // Pattern indexes for fast lookup (exact-key indexes)
  std::map<std::string, std::vector<std::string>> url_pattern_index_;
  std::map<std::string, std::vector<std::string>> source_pattern_index_;

  // Subscription ID generation
  int next_subscription_id_ = 1;
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_STREAMING_STREAM_REGISTRY_H_
