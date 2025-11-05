// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/storage/circular_event_buffer.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"

namespace datasipper {

CircularEventBuffer::CircularEventBuffer(size_t capacity)
    : capacity_(capacity) {
  DCHECK_GT(capacity, 0);
  buffer_.resize(capacity_);
  DVLOG(1) << "Created CircularEventBuffer with capacity: " << capacity_;
}

CircularEventBuffer::~CircularEventBuffer() {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "Destroying CircularEventBuffer. Total events processed: "
           << total_events_added_;
}

void CircularEventBuffer::AddEvent(std::unique_ptr<NetworkEvent> event) {
  if (!event) {
    LOG(WARNING) << "Attempted to add null event to buffer";
    return;
  }

  base::AutoLock auto_lock(lock_);

  // Calculate insertion index
  size_t insert_index = (head_index_ + size_) % capacity_;

  // Check if we're about to evict an event
  std::unique_ptr<NetworkEvent> evicted_event;
  if (size_ == capacity_) {
    // Buffer is full, we'll evict the oldest event
    evicted_event = std::move(buffer_[head_index_].event);
    total_events_evicted_++;

    // Move head forward
    head_index_ = (head_index_ + 1) % capacity_;
  } else {
    // Buffer has space
    size_++;
  }

  // Insert new event
  buffer_[insert_index].event = std::move(event);
  buffer_[insert_index].sequence_number = ++sequence_counter_;
  buffer_[insert_index].is_valid = true;

  total_events_added_++;

  // Notify observers (outside lock to avoid deadlock)
  NetworkEvent* added_event = buffer_[insert_index].event.get();
  std::vector<CircularEventBufferObserver*> observers_copy = observers_;

  lock_.Release();

  // Notify about evicted event first
  if (evicted_event) {
    for (auto* observer : observers_copy) {
      observer->OnEventEvicted(*evicted_event);
    }
  }

  // Notify about added event
  for (auto* observer : observers_copy) {
    observer->OnEventAdded(*added_event);
  }

  // Notify if buffer became full
  if (size_ == capacity_) {
    for (auto* observer : observers_copy) {
      observer->OnBufferFull();
    }
  }

  lock_.Acquire();
}

std::vector<std::unique_ptr<NetworkEvent>>
CircularEventBuffer::GetEvents(size_t max_count) const {
  base::AutoLock auto_lock(lock_);

  std::vector<std::unique_ptr<NetworkEvent>> events;

  if (size_ == 0) {
    return events;
  }

  size_t count = (max_count > 0) ? std::min(max_count, size_) : size_;
  events.reserve(count);

  // Start from the oldest event (head) and go forward
  for (size_t i = 0; i < count; ++i) {
    size_t index = (head_index_ + i) % capacity_;
    if (buffer_[index].is_valid && buffer_[index].event) {
      // Create a copy of the event
      auto event_copy = std::make_unique<NetworkEvent>();
      *event_copy = *buffer_[index].event;  // Copy constructor
      events.push_back(std::move(event_copy));
    }
  }

  return events;
}

std::vector<std::unique_ptr<NetworkEvent>>
CircularEventBuffer::GetEventsSince(base::Time since) const {
  base::AutoLock auto_lock(lock_);

  std::vector<std::unique_ptr<NetworkEvent>> events;

  if (size_ == 0) {
    return events;
  }

  // Iterate through all events and filter by timestamp
  for (size_t i = 0; i < size_; ++i) {
    size_t index = (head_index_ + i) % capacity_;
    if (buffer_[index].is_valid && buffer_[index].event &&
        buffer_[index].event->timestamp >= since) {
      auto event_copy = std::make_unique<NetworkEvent>();
      *event_copy = *buffer_[index].event;
      events.push_back(std::move(event_copy));
    }
  }

  return events;
}

std::unique_ptr<NetworkEvent>
CircularEventBuffer::GetEvent(const std::string& event_id) const {
  base::AutoLock auto_lock(lock_);

  for (size_t i = 0; i < size_; ++i) {
    size_t index = (head_index_ + i) % capacity_;
    if (buffer_[index].is_valid && buffer_[index].event &&
        buffer_[index].event->id == event_id) {
      auto event_copy = std::make_unique<NetworkEvent>();
      *event_copy = *buffer_[index].event;
      return event_copy;
    }
  }

  return nullptr;
}

std::vector<std::unique_ptr<NetworkEvent>>
CircularEventBuffer::GetEventsByStream(const std::string& stream_name,
                                       size_t max_count) const {
  base::AutoLock auto_lock(lock_);

  std::vector<std::unique_ptr<NetworkEvent>> events;

  if (size_ == 0 || stream_name.empty()) {
    return events;
  }

  size_t count = 0;
  for (size_t i = 0; i < size_; ++i) {
    if (max_count > 0 && count >= max_count) {
      break;
    }

    size_t index = (head_index_ + i) % capacity_;
    if (buffer_[index].is_valid && buffer_[index].event &&
        buffer_[index].event->stream_name == stream_name) {
      auto event_copy = std::make_unique<NetworkEvent>();
      *event_copy = *buffer_[index].event;
      events.push_back(std::move(event_copy));
      count++;
    }
  }

  return events;
}

std::vector<std::unique_ptr<NetworkEvent>>
CircularEventBuffer::GetEventsByGroup(const std::string& group_name,
                                      size_t max_count) const {
  base::AutoLock auto_lock(lock_);

  std::vector<std::unique_ptr<NetworkEvent>> events;

  if (size_ == 0 || group_name.empty()) {
    return events;
  }

  size_t count = 0;
  for (size_t i = 0; i < size_; ++i) {
    if (max_count > 0 && count >= max_count) {
      break;
    }

    size_t index = (head_index_ + i) % capacity_;
    if (buffer_[index].is_valid && buffer_[index].event &&
        buffer_[index].event->group_name == group_name) {
      auto event_copy = std::make_unique<NetworkEvent>();
      *event_copy = *buffer_[index].event;
      events.push_back(std::move(event_copy));
      count++;
    }
  }

  return events;
}

std::vector<std::unique_ptr<NetworkEvent>>
CircularEventBuffer::GetEventsByUrl(const std::string& url_pattern,
                                    size_t max_count) const {
  base::AutoLock auto_lock(lock_);

  std::vector<std::unique_ptr<NetworkEvent>> events;

  if (size_ == 0 || url_pattern.empty()) {
    return events;
  }

  size_t count = 0;
  for (size_t i = 0; i < size_; ++i) {
    if (max_count > 0 && count >= max_count) {
      break;
    }

    size_t index = (head_index_ + i) % capacity_;
    if (buffer_[index].is_valid && buffer_[index].event &&
        MatchesUrlPattern(buffer_[index].event->url.spec(), url_pattern)) {
      auto event_copy = std::make_unique<NetworkEvent>();
      *event_copy = *buffer_[index].event;
      events.push_back(std::move(event_copy));
      count++;
    }
  }

  return events;
}

size_t CircularEventBuffer::GetSize() const {
  base::AutoLock auto_lock(lock_);
  return size_;
}

size_t CircularEventBuffer::GetTotalEventsAdded() const {
  base::AutoLock auto_lock(lock_);
  return total_events_added_;
}

size_t CircularEventBuffer::GetTotalEventsEvicted() const {
  base::AutoLock auto_lock(lock_);
  return total_events_evicted_;
}

base::Time CircularEventBuffer::GetOldestEventTime() const {
  base::AutoLock auto_lock(lock_);

  if (size_ == 0) {
    return base::Time();
  }

  if (buffer_[head_index_].is_valid && buffer_[head_index_].event) {
    return buffer_[head_index_].event->timestamp;
  }

  return base::Time();
}

base::Time CircularEventBuffer::GetNewestEventTime() const {
  base::AutoLock auto_lock(lock_);

  if (size_ == 0) {
    return base::Time();
  }

  // Find the newest event (highest sequence number)
  size_t newest_index = head_index_;
  size_t highest_sequence = 0;

  for (size_t i = 0; i < size_; ++i) {
    size_t index = (head_index_ + i) % capacity_;
    if (buffer_[index].is_valid && buffer_[index].sequence_number > highest_sequence) {
      highest_sequence = buffer_[index].sequence_number;
      newest_index = index;
    }
  }

  if (buffer_[newest_index].event) {
    return buffer_[newest_index].event->timestamp;
  }

  return base::Time();
}

void CircularEventBuffer::Clear() {
  base::AutoLock auto_lock(lock_);

  for (auto& slot : buffer_) {
    slot.event.reset();
    slot.is_valid = false;
    slot.sequence_number = 0;
  }

  head_index_ = 0;
  size_ = 0;

  DVLOG(1) << "CircularEventBuffer cleared";
}

void CircularEventBuffer::AddObserver(CircularEventBufferObserver* observer) {
  base::AutoLock auto_lock(lock_);
  observers_.push_back(observer);
}

void CircularEventBuffer::RemoveObserver(CircularEventBufferObserver* observer) {
  base::AutoLock auto_lock(lock_);
  auto it = std::find(observers_.begin(), observers_.end(), observer);
  if (it != observers_.end()) {
    observers_.erase(it);
  }
}

bool CircularEventBuffer::MatchesUrlPattern(const std::string& url,
                                           const std::string& pattern) const {
  // Simple wildcard matching for now
  // TODO: Implement more sophisticated pattern matching
  if (pattern == "*") {
    return true;
  }

  if (pattern.find('*') == std::string::npos) {
    // No wildcards, do exact match
    return url == pattern;
  }

  // Basic wildcard support
  return base::MatchPattern(url, pattern);
}

}  // namespace datasipper
