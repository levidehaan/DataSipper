// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_STREAMING_STREAM_DATABASE_SYNC_H_
#define COMPONENTS_DATASIPPER_STREAMING_STREAM_DATABASE_SYNC_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/datasipper/streaming/stream_registry.h"
#include "components/datasipper/streaming/stream_types.h"

namespace sql {
class Database;
}

namespace datasipper {

class StreamRegistry;
struct StreamDefinition;
struct StreamEvent;

// Synchronizes StreamRegistry with database storage
// Provides persistence for detected streams and their events
class StreamDatabaseSync {
 public:
  explicit StreamDatabaseSync(sql::Database* db, StreamRegistry* registry);
  ~StreamDatabaseSync();

  StreamDatabaseSync(const StreamDatabaseSync&) = delete;
  StreamDatabaseSync& operator=(const StreamDatabaseSync&) = delete;

  // Initialize database tables for streams
  bool Initialize();

  // Load streams from database into registry
  bool LoadStreamsFromDatabase();

  // Stream operations - save/update/delete
  bool SaveStream(const StreamDefinition& stream);
  bool UpdateStream(const std::string& stream_id,
                    const StreamDefinition& stream);
  bool DeleteStream(const std::string& stream_id);

  // Event storage
  bool SaveEvent(const StreamEvent& event);
  std::vector<StreamEvent> GetStreamEvents(const std::string& stream_id,
                                            int limit = 100) const;
  std::vector<StreamEvent> GetRecentEvents(const std::string& stream_id,
                                            base::Time since) const;

  // Subscription persistence (for future use)
  bool SaveSubscription(const std::string& stream_id,
                        const std::string& subscriber_type,
                        const std::string& subscriber_data);
  bool LoadSubscriptions();
  bool DeleteSubscription(const std::string& subscription_id);

  // Cleanup operations
  bool DeleteOldEvents(base::TimeDelta older_than);
  bool DeleteInactiveStreams(base::TimeDelta inactive_for);

  // Statistics
  int GetStreamCount() const;
  int GetEventCount() const;
  int GetActiveStreamCount() const;

 private:
  bool CreateTables();
  bool CreateStreamTable();
  bool CreateEventTable();
  bool CreateSubscriptionTable();
  bool CreateIndexes();

  bool MigrateTables();

  raw_ptr<sql::Database> db_;
  raw_ptr<StreamRegistry> registry_;
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_STREAMING_STREAM_DATABASE_SYNC_H_
