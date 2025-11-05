// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/streaming/stream_registry.h"

#include <algorithm>

#include "base/logging.h"

namespace datasipper {

StreamEvent::StreamEvent()
    : timestamp(),
      data(base::Value(base::Value::Dict())) {}
StreamEvent::~StreamEvent() = default;
StreamEvent::StreamEvent(const StreamEvent& other)
    : stream_id(other.stream_id),
      timestamp(other.timestamp),
      data(other.data.Clone()),
      event_type(other.event_type) {}
StreamEvent& StreamEvent::operator=(const StreamEvent& other) {
  if (this != &other) {
    stream_id = other.stream_id;
    timestamp = other.timestamp;
    data = other.data.Clone();
    event_type = other.event_type;
  }
  return *this;
}
StreamEvent::StreamEvent(StreamEvent&&) noexcept = default;
StreamEvent& StreamEvent::operator=(StreamEvent&&) noexcept = default;

StreamSubscriptionOptions::StreamSubscriptionOptions() = default;
StreamSubscriptionOptions::~StreamSubscriptionOptions() = default;

StreamRegistry::StreamRegistry() = default;
StreamRegistry::~StreamRegistry() = default;

// Inner struct definitions (Chromium style requires out-of-line for complex types)
StreamRegistry::ManagedStream::ManagedStream() = default;
StreamRegistry::ManagedStream::~ManagedStream() = default;

StreamRegistry::Subscriber::Subscriber() = default;
StreamRegistry::Subscriber::~Subscriber() = default;

std::string StreamRegistry::RegisterStream(const StreamDefinition& stream) {
  std::optional<std::string> existing_id = FindDuplicateStream(stream);
  if (existing_id.has_value()) {
    UpdateStream(existing_id.value(), stream);
    return existing_id.value();
  }

  ManagedStream managed;
  managed.definition = stream;
  managed.registered_at = base::Time::Now();
  managed.last_update = managed.registered_at;
  managed.is_active = stream.is_active;
  managed.event_count_total = 0;

  std::string stream_id = stream.stream_id;
  streams_[stream_id] = std::move(managed);

  IndexStreamPattern(stream_id, stream);

  LOG(INFO) << "StreamRegistry: Registered stream " << stream_id
            << " (total: " << streams_.size() << ")";
  return stream_id;
}

void StreamRegistry::UpdateStream(const std::string& stream_id,
                                  const StreamDefinition& updated_stream) {
  auto it = streams_.find(stream_id);
  if (it == streams_.end()) {
    return;
  }

  RemoveStreamFromIndex(stream_id);
  it->second.definition = updated_stream;
  it->second.last_update = base::Time::Now();
  it->second.is_active = updated_stream.is_active;
  IndexStreamPattern(stream_id, updated_stream);
}

void StreamRegistry::RemoveStream(const std::string& stream_id) {
  RemoveStreamFromIndex(stream_id);

  std::vector<std::string> to_remove;
  for (const auto& [sub_id, sub] : subscribers_) {
    if (sub.stream_id == stream_id) {
      to_remove.push_back(sub_id);
    }
  }
  for (const auto& id : to_remove) {
    subscribers_.erase(id);
  }

  streams_.erase(stream_id);
}

std::optional<StreamDefinition> StreamRegistry::GetStream(
    const std::string& stream_id) const {
  auto it = streams_.find(stream_id);
  if (it == streams_.end()) {
    return std::nullopt;
  }
  return it->second.definition;
}

std::vector<StreamDefinition> StreamRegistry::GetAllStreams() const {
  std::vector<StreamDefinition> result;
  result.reserve(streams_.size());
  for (const auto& [id, managed] : streams_) {
    result.push_back(managed.definition);
  }
  return result;
}

std::vector<StreamDefinition> StreamRegistry::GetActiveStreams() const {
  std::vector<StreamDefinition> result;
  for (const auto& [id, managed] : streams_) {
    if (managed.is_active) {
      result.push_back(managed.definition);
    }
  }
  return result;
}

std::vector<StreamDefinition> StreamRegistry::GetStreamsByType(
    StreamType type) const {
  std::vector<StreamDefinition> result;
  for (const auto& [id, managed] : streams_) {
    if (managed.definition.type == type) {
      result.push_back(managed.definition);
    }
  }
  return result;
}

std::vector<std::string> StreamRegistry::GetStreamIdsByPattern(
    const std::string& pattern) const {
  std::vector<std::string> result;
  auto it1 = url_pattern_index_.find(pattern);
  if (it1 != url_pattern_index_.end()) {
    result.insert(result.end(), it1->second.begin(), it1->second.end());
  }
  auto it2 = source_pattern_index_.find(pattern);
  if (it2 != source_pattern_index_.end()) {
    result.insert(result.end(), it2->second.begin(), it2->second.end());
  }
  return result;
}

void StreamRegistry::SetStreamActive(const std::string& stream_id, bool active) {
  auto it = streams_.find(stream_id);
  if (it == streams_.end()) {
    return;
  }
  it->second.is_active = active;
  it->second.last_update = base::Time::Now();
}

bool StreamRegistry::IsStreamActive(const std::string& stream_id) const {
  auto it = streams_.find(stream_id);
  if (it == streams_.end()) {
    return false;
  }
  return it->second.is_active;
}

void StreamRegistry::PublishEvent(const std::string& stream_id,
                                  const base::Value& data,
                                  const std::string& event_type) {
  auto it = streams_.find(stream_id);
  if (it == streams_.end()) {
    LOG(WARNING) << "StreamRegistry: publish to unknown stream: " << stream_id;
    return;
  }

  ManagedStream& managed = it->second;

  StreamEvent event;
  event.stream_id = stream_id;
  event.timestamp = base::Time::Now();
  event.data = data.Clone();
  event.event_type = event_type;

  managed.recent_events.push_back(event);
  if (managed.recent_events.size() > ManagedStream::kMaxRecentEvents) {
    managed.recent_events.pop_front();
  }

  managed.event_count_total++;
  managed.last_update = event.timestamp;

  NotifySubscribers(stream_id, event);
}

std::string StreamRegistry::Subscribe(const std::string& stream_id,
                                      StreamEventCallback callback,
                                      const StreamSubscriptionOptions& options) {
  if (streams_.find(stream_id) == streams_.end()) {
    LOG(ERROR) << "StreamRegistry: subscribe to unknown stream: " << stream_id;
    return std::string();
  }

  Subscriber subscriber;
  subscriber.subscriber_id =
      std::string("sub_") + stream_id + "_" + std::to_string(next_subscription_id_++);
  subscriber.stream_id = stream_id;
  subscriber.callback = std::move(callback);
  subscriber.options = options;
  subscriber.subscribed_at = base::Time::Now();
  subscriber.events_received = 0;
  subscriber.last_event_time = base::Time::Min();

  if (options.include_recent_events && options.recent_event_count > 0) {
    std::vector<StreamEvent> recent = GetRecentEvents(stream_id, options.recent_event_count);
    for (const auto& e : recent) {
      subscriber.callback.Run(e);
      subscriber.events_received++;
    }
  }

  std::string subscription_id = subscriber.subscriber_id;
  subscribers_[subscription_id] = std::move(subscriber);

  LOG(INFO) << "StreamRegistry: Subscribed " << subscription_id
            << " to stream " << stream_id;

  return subscription_id;
}

void StreamRegistry::Unsubscribe(const std::string& subscription_id) {
  subscribers_.erase(subscription_id);
}

int StreamRegistry::GetSubscriberCount(const std::string& stream_id) const {
  int count = 0;
  for (const auto& [id, sub] : subscribers_) {
    if (sub.stream_id == stream_id) {
      ++count;
    }
  }
  return count;
}

std::vector<std::string> StreamRegistry::MatchUrlPattern(
    const std::string& url_pattern) const {
  auto it = url_pattern_index_.find(url_pattern);
  if (it == url_pattern_index_.end()) {
    return {};
  }
  return it->second;
}

std::vector<std::string> StreamRegistry::MatchSourcePattern(
    const std::string& source_pattern) const {
  auto it = source_pattern_index_.find(source_pattern);
  if (it == source_pattern_index_.end()) {
    return {};
  }
  return it->second;
}

size_t StreamRegistry::GetActiveStreamCount() const {
  size_t count = 0;
  for (const auto& [id, managed] : streams_) {
    if (managed.is_active) {
      ++count;
    }
  }
  return count;
}

size_t StreamRegistry::GetTotalEventCount() const {
  size_t total = 0;
  for (const auto& [id, managed] : streams_) {
    total += static_cast<size_t>(managed.event_count_total);
  }
  return total;
}

std::vector<StreamEvent> StreamRegistry::GetRecentEvents(
    const std::string& stream_id, int count) const {
  std::vector<StreamEvent> result;
  auto it = streams_.find(stream_id);
  if (it == streams_.end() || count <= 0) {
    return result;
  }
  const auto& buf = it->second.recent_events;
  int n = std::min<int>(count, static_cast<int>(buf.size()));
  result.reserve(n);
  for (int i = static_cast<int>(buf.size()) - n; i < static_cast<int>(buf.size()); ++i) {
    result.push_back(buf[i]);
  }
  return result;
}

std::optional<std::string> StreamRegistry::FindDuplicateStream(
    const StreamDefinition& stream) const {
  for (const auto& [id, managed] : streams_) {
    const StreamDefinition& existing = managed.definition;

    if (existing.stream_id == stream.stream_id) {
      return id;
    }

    if (existing.type == stream.type &&
        existing.source_pattern == stream.source_pattern) {
      if (stream.type == StreamType::DOM_MUTATION) {
        if (existing.url == stream.url) {
          return id;
        }
      } else {
        return id;
      }
    }
  }
  return std::nullopt;
}

void StreamRegistry::NotifySubscribers(const std::string& stream_id,
                                       const StreamEvent& event) {
  std::vector<std::string> to_remove;
  for (auto& [sub_id, subscriber] : subscribers_) {
    if (subscriber.stream_id != stream_id) {
      continue;
    }

    if (!ShouldDeliverEvent(subscriber, event)) {
      continue;
    }

    subscriber.callback.Run(event);
    subscriber.events_received++;
    subscriber.last_event_time = event.timestamp;

    if (subscriber.options.max_events.has_value() &&
        subscriber.events_received >= subscriber.options.max_events.value()) {
      to_remove.push_back(sub_id);
    }

    if (subscriber.options.max_duration.has_value()) {
      base::TimeDelta elapsed = base::Time::Now() - subscriber.subscribed_at;
      if (elapsed >= subscriber.options.max_duration.value()) {
        to_remove.push_back(sub_id);
      }
    }
  }

  for (const auto& id : to_remove) {
    Unsubscribe(id);
  }
}

bool StreamRegistry::ShouldDeliverEvent(const Subscriber& subscriber,
                                        const StreamEvent& event) const {
  if (subscriber.options.event_type_filter.has_value() &&
      subscriber.options.event_type_filter.value() != event.event_type) {
    return false;
  }

  if (subscriber.options.min_interval.has_value()) {
    if (!subscriber.last_event_time.is_null() &&
        subscriber.last_event_time != base::Time::Min()) {
      base::TimeDelta delta = event.timestamp - subscriber.last_event_time;
      if (delta < subscriber.options.min_interval.value()) {
        return false;
      }
    }
  }

  return true;
}

void StreamRegistry::CheckSubscriberLimits(const std::string& subscription_id) {
  auto it = subscribers_.find(subscription_id);
  if (it == subscribers_.end()) {
    return;
  }
  const Subscriber& s = it->second;
  if (s.options.max_events.has_value() && s.events_received >= s.options.max_events.value()) {
    Unsubscribe(subscription_id);
    return;
  }
  if (s.options.max_duration.has_value()) {
    base::TimeDelta elapsed = base::Time::Now() - s.subscribed_at;
    if (elapsed >= s.options.max_duration.value()) {
      Unsubscribe(subscription_id);
    }
  }
}

void StreamRegistry::IndexStreamPattern(const std::string& stream_id,
                                        const StreamDefinition& stream) {
  if (!stream.url.empty()) {
    auto& vec = url_pattern_index_[stream.url];
    if (std::find(vec.begin(), vec.end(), stream_id) == vec.end()) {
      vec.push_back(stream_id);
    }
  }
  if (!stream.source_pattern.empty()) {
    auto& vec = source_pattern_index_[stream.source_pattern];
    if (std::find(vec.begin(), vec.end(), stream_id) == vec.end()) {
      vec.push_back(stream_id);
    }
  }
}

void StreamRegistry::RemoveStreamFromIndex(const std::string& stream_id) {
  for (auto& [key, vec] : url_pattern_index_) {
    vec.erase(std::remove(vec.begin(), vec.end(), stream_id), vec.end());
  }
  for (auto& [key, vec] : source_pattern_index_) {
    vec.erase(std::remove(vec.begin(), vec.end(), stream_id), vec.end());
  }
}

}  // namespace datasipper
