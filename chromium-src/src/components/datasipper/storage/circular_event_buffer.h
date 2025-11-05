// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_STORAGE_CIRCULAR_EVENT_BUFFER_H_
#define COMPONENTS_DATASIPPER_STORAGE_CIRCULAR_EVENT_BUFFER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "components/datasipper/common/network_event.h"
#include "url/gurl.h"

namespace datasipper {

// Observer interface for circular event buffer
class CircularEventBufferObserver {
 public:
  virtual ~CircularEventBufferObserver() = default;

  virtual void OnEventAdded(const NetworkEvent& event) = 0;
  virtual void OnEventEvicted(const NetworkEvent& event) = 0;
  virtual void OnBufferFull() = 0;
};

// Thread-safe circular buffer for network events
class CircularEventBuffer {
 public:
  explicit CircularEventBuffer(size_t capacity);
  ~CircularEventBuffer();

  CircularEventBuffer(const CircularEventBuffer&) = delete;
  CircularEventBuffer& operator=(const CircularEventBuffer&) = delete;

  void AddEvent(std::unique_ptr<NetworkEvent> event);

  std::vector<std::unique_ptr<NetworkEvent>> GetEvents(size_t max_count = 0) const;
  std::vector<std::unique_ptr<NetworkEvent>> GetEventsSince(base::Time since) const;
  std::unique_ptr<NetworkEvent> GetEvent(const std::string& event_id) const;
  std::vector<std::unique_ptr<NetworkEvent>> GetEventsByStream(const std::string& stream_name, size_t max_count = 0) const;
  std::vector<std::unique_ptr<NetworkEvent>> GetEventsByGroup(const std::string& group_name, size_t max_count = 0) const;
  std::vector<std::unique_ptr<NetworkEvent>> GetEventsByUrl(const std::string& url_pattern, size_t max_count = 0) const;

  size_t GetSize() const;
  size_t GetTotalEventsAdded() const;
  size_t GetTotalEventsEvicted() const;
  base::Time GetOldestEventTime() const;
  base::Time GetNewestEventTime() const;

  void Clear();

  void AddObserver(CircularEventBufferObserver* observer);
  void RemoveObserver(CircularEventBufferObserver* observer);

 private:
  struct EventSlot {
    std::unique_ptr<NetworkEvent> event;
    size_t sequence_number = 0;
    bool is_valid = false;
  };

  bool MatchesUrlPattern(const std::string& url, const std::string& pattern) const;

  mutable base::Lock lock_;
  std::vector<EventSlot> buffer_;
  size_t capacity_;
  size_t head_index_ = 0;
  size_t size_ = 0;
  size_t sequence_counter_ = 0;
  size_t total_events_added_ = 0;
  size_t total_events_evicted_ = 0;
  std::vector<CircularEventBufferObserver*> observers_;
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_STORAGE_CIRCULAR_EVENT_BUFFER_H_
