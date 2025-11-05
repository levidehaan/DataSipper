// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/streaming/stream_database_sync.h"

#include "base/json/json_writer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace datasipper {

StreamDatabaseSync::StreamDatabaseSync(sql::Database* db,
                                        StreamRegistry* registry)
    : db_(db), registry_(registry) {}

StreamDatabaseSync::~StreamDatabaseSync() = default;

bool StreamDatabaseSync::Initialize() {
  if (!db_ || !registry_) {
    LOG(ERROR) << "StreamDatabaseSync: null db or registry";
    return false;
  }

  if (!CreateTables()) {
    LOG(ERROR) << "StreamDatabaseSync: Failed to create tables";
    return false;
  }

  if (!LoadStreamsFromDatabase()) {
    LOG(WARNING) << "StreamDatabaseSync: Failed to load streams";
    // Non-fatal, continue
  }

  LOG(INFO) << " StreamDatabaseSync: Initialized";
  return true;
}

bool StreamDatabaseSync::CreateTables() {
  return CreateStreamTable() && CreateEventTable() &&
         CreateSubscriptionTable() && CreateIndexes();
}

bool StreamDatabaseSync::CreateStreamTable() {
  const char kSql[] =
      "CREATE TABLE IF NOT EXISTS detected_streams ("
      "  stream_id TEXT PRIMARY KEY,"
      "  label TEXT NOT NULL,"
      "  stream_type INTEGER NOT NULL,"
      "  source_pattern TEXT NOT NULL,"
      "  url TEXT,"
      "  first_seen INTEGER NOT NULL,"
      "  last_seen INTEGER NOT NULL,"
      "  avg_interval_ms REAL,"
      "  std_deviation REAL,"
      "  is_regular INTEGER,"
      "  is_active INTEGER NOT NULL,"
      "  event_count INTEGER DEFAULT 0,"
      "  data_type INTEGER,"
      "  registered_at INTEGER NOT NULL,"
      "  last_update INTEGER NOT NULL"
      ")";

  return db_->Execute(kSql);
}

bool StreamDatabaseSync::CreateEventTable() {
  const char kSql[] =
      "CREATE TABLE IF NOT EXISTS stream_events ("
      "  event_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  stream_id TEXT NOT NULL,"
      "  timestamp INTEGER NOT NULL,"
      "  event_type TEXT NOT NULL,"
      "  data TEXT,"
      "  FOREIGN KEY(stream_id) REFERENCES detected_streams(stream_id)"
      "    ON DELETE CASCADE"
      ")";

  return db_->Execute(kSql);
}

bool StreamDatabaseSync::CreateSubscriptionTable() {
  const char kSql[] =
      "CREATE TABLE IF NOT EXISTS stream_subscriptions ("
      "  subscription_id TEXT PRIMARY KEY,"
      "  stream_id TEXT NOT NULL,"
      "  subscriber_type TEXT NOT NULL,"
      "  subscriber_data TEXT,"
      "  created_at INTEGER NOT NULL,"
      "  is_active INTEGER NOT NULL,"
      "  events_received INTEGER DEFAULT 0,"
      "  FOREIGN KEY(stream_id) REFERENCES detected_streams(stream_id)"
      "    ON DELETE CASCADE"
      ")";

  return db_->Execute(kSql);
}

bool StreamDatabaseSync::CreateIndexes() {
  const char* kIndexes[] = {
      "CREATE INDEX IF NOT EXISTS idx_streams_type "
      "ON detected_streams(stream_type)",

      "CREATE INDEX IF NOT EXISTS idx_streams_active "
      "ON detected_streams(is_active)",

      "CREATE INDEX IF NOT EXISTS idx_streams_pattern "
      "ON detected_streams(source_pattern)",

      "CREATE INDEX IF NOT EXISTS idx_streams_url ON detected_streams(url)",

      "CREATE INDEX IF NOT EXISTS idx_events_stream "
      "ON stream_events(stream_id)",

      "CREATE INDEX IF NOT EXISTS idx_events_timestamp "
      "ON stream_events(timestamp)",

      "CREATE INDEX IF NOT EXISTS idx_events_type "
      "ON stream_events(stream_id, event_type)",

      "CREATE INDEX IF NOT EXISTS idx_subscriptions_stream "
      "ON stream_subscriptions(stream_id)",

      "CREATE INDEX IF NOT EXISTS idx_subscriptions_active "
      "ON stream_subscriptions(is_active)",
  };

  for (const char* sql : kIndexes) {
    if (!db_->Execute(sql)) {
      return false;
    }
  }
  return true;
}

bool StreamDatabaseSync::LoadStreamsFromDatabase() {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT stream_id, label, stream_type, source_pattern, url, "
      "       first_seen, last_seen, avg_interval_ms, std_deviation, "
      "       is_regular, is_active, event_count, data_type "
      "FROM detected_streams"));

  int loaded = 0;
  while (statement.Step()) {
    StreamDefinition stream;

    stream.stream_id = statement.ColumnString(0);
    stream.label = statement.ColumnString(1);
    stream.type = static_cast<StreamType>(statement.ColumnInt(2));
    stream.source_pattern = statement.ColumnString(3);
    stream.url = statement.ColumnString(4);

    stream.first_seen = base::Time::FromMillisecondsSinceUnixEpoch(
        statement.ColumnInt64(5));
    stream.last_seen =
        base::Time::FromMillisecondsSinceUnixEpoch(statement.ColumnInt64(6));

    stream.timing.avg_interval_ms = statement.ColumnDouble(7);
    stream.timing.std_deviation = statement.ColumnDouble(8);
    stream.timing.is_regular = statement.ColumnBool(9);
    stream.is_active = statement.ColumnBool(10);
    stream.event_count = statement.ColumnInt(11);
    stream.data_type = static_cast<DataType>(statement.ColumnInt(12));

    registry_->RegisterStream(stream);
    loaded++;
  }

  LOG(INFO) << " StreamDatabaseSync: Loaded " << loaded << " streams";
  return statement.Succeeded();
}

bool StreamDatabaseSync::SaveStream(const StreamDefinition& stream) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO detected_streams "
      "(stream_id, label, stream_type, source_pattern, url, "
      " first_seen, last_seen, avg_interval_ms, std_deviation, "
      " is_regular, is_active, event_count, data_type, "
      " registered_at, last_update) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

  statement.BindString(0, stream.stream_id);
  statement.BindString(1, stream.label);
  statement.BindInt(2, static_cast<int>(stream.type));
  statement.BindString(3, stream.source_pattern);
  statement.BindString(4, stream.url);
  statement.BindInt64(5, stream.first_seen.InMillisecondsSinceUnixEpoch());
  statement.BindInt64(6, stream.last_seen.InMillisecondsSinceUnixEpoch());
  statement.BindDouble(7, stream.timing.avg_interval_ms);
  statement.BindDouble(8, stream.timing.std_deviation);
  statement.BindBool(9, stream.timing.is_regular);
  statement.BindBool(10, stream.is_active);
  statement.BindInt(11, stream.event_count);
  statement.BindInt(12, static_cast<int>(stream.data_type));
  statement.BindInt64(13, base::Time::Now().InMillisecondsSinceUnixEpoch());
  statement.BindInt64(14, base::Time::Now().InMillisecondsSinceUnixEpoch());

  return statement.Run();
}

bool StreamDatabaseSync::UpdateStream(const std::string& stream_id,
                                       const StreamDefinition& stream) {
  return SaveStream(stream);
}

bool StreamDatabaseSync::DeleteStream(const std::string& stream_id) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM detected_streams WHERE stream_id = ?"));
  statement.BindString(0, stream_id);
  return statement.Run();
}

bool StreamDatabaseSync::SaveEvent(const StreamEvent& event) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO stream_events "
      "(stream_id, timestamp, event_type, data) "
      "VALUES (?, ?, ?, ?)"));

  statement.BindString(0, event.stream_id);
  statement.BindInt64(1, event.timestamp.InMillisecondsSinceUnixEpoch());
  statement.BindString(2, event.event_type);

  std::string json_data;
  base::JSONWriter::Write(event.data, &json_data);
  statement.BindString(3, json_data);

  return statement.Run();
}

std::vector<StreamEvent> StreamDatabaseSync::GetStreamEvents(
    const std::string& stream_id,
    int limit) const {
  std::vector<StreamEvent> result;

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT stream_id, timestamp, event_type, data "
      "FROM stream_events "
      "WHERE stream_id = ? "
      "ORDER BY timestamp DESC "
      "LIMIT ?"));

  statement.BindString(0, stream_id);
  statement.BindInt(1, limit);

  while (statement.Step()) {
    StreamEvent event;
    event.stream_id = statement.ColumnString(0);
    event.timestamp =
        base::Time::FromMillisecondsSinceUnixEpoch(statement.ColumnInt64(1));
    event.event_type = statement.ColumnString(2);

    std::string json_data = statement.ColumnString(3);
    auto parsed = base::JSONReader::Read(json_data);
    if (parsed.has_value()) {
      event.data = std::move(parsed.value());
    } else {
      event.data = base::Value(base::Value::Dict());
    }

    result.push_back(std::move(event));
  }

  return result;
}

std::vector<StreamEvent> StreamDatabaseSync::GetRecentEvents(
    const std::string& stream_id,
    base::Time since) const {
  std::vector<StreamEvent> result;

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT stream_id, timestamp, event_type, data "
      "FROM stream_events "
      "WHERE stream_id = ? AND timestamp >= ? "
      "ORDER BY timestamp DESC"));

  statement.BindString(0, stream_id);
  statement.BindInt64(1, since.InMillisecondsSinceUnixEpoch());

  while (statement.Step()) {
    StreamEvent event;
    event.stream_id = statement.ColumnString(0);
    event.timestamp =
        base::Time::FromMillisecondsSinceUnixEpoch(statement.ColumnInt64(1));
    event.event_type = statement.ColumnString(2);

    std::string json_data = statement.ColumnString(3);
    auto parsed = base::JSONReader::Read(json_data);
    if (parsed.has_value()) {
      event.data = std::move(parsed.value());
    } else {
      event.data = base::Value(base::Value::Dict());
    }

    result.push_back(std::move(event));
  }

  return result;
}

bool StreamDatabaseSync::SaveSubscription(
    const std::string& stream_id,
    const std::string& subscriber_type,
    const std::string& subscriber_data) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO stream_subscriptions "
      "(subscription_id, stream_id, subscriber_type, subscriber_data, "
      " created_at, is_active, events_received) "
      "VALUES (?, ?, ?, ?, ?, ?, ?)"));

  std::string sub_id = "sub_" + stream_id + "_" +
                       std::to_string(base::Time::Now().ToTimeT());
  statement.BindString(0, sub_id);
  statement.BindString(1, stream_id);
  statement.BindString(2, subscriber_type);
  statement.BindString(3, subscriber_data);
  statement.BindInt64(4, base::Time::Now().InMillisecondsSinceUnixEpoch());
  statement.BindBool(5, true);
  statement.BindInt(6, 0);

  return statement.Run();
}

bool StreamDatabaseSync::LoadSubscriptions() {
  // Future implementation for restoring subscriptions on startup
  return true;
}

bool StreamDatabaseSync::DeleteSubscription(
    const std::string& subscription_id) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM stream_subscriptions WHERE subscription_id = ?"));
  statement.BindString(0, subscription_id);
  return statement.Run();
}

bool StreamDatabaseSync::DeleteOldEvents(base::TimeDelta older_than) {
  base::Time cutoff = base::Time::Now() - older_than;

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM stream_events WHERE timestamp < ?"));
  statement.BindInt64(0, cutoff.InMillisecondsSinceUnixEpoch());

  return statement.Run();
}

bool StreamDatabaseSync::DeleteInactiveStreams(base::TimeDelta inactive_for) {
  base::Time cutoff = base::Time::Now() - inactive_for;

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM detected_streams "
      "WHERE is_active = 0 AND last_update < ?"));
  statement.BindInt64(0, cutoff.InMillisecondsSinceUnixEpoch());

  return statement.Run();
}

int StreamDatabaseSync::GetStreamCount() const {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT COUNT(*) FROM detected_streams"));

  if (statement.Step()) {
    return statement.ColumnInt(0);
  }
  return 0;
}

int StreamDatabaseSync::GetEventCount() const {
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, "SELECT COUNT(*) FROM stream_events"));

  if (statement.Step()) {
    return statement.ColumnInt(0);
  }
  return 0;
}

int StreamDatabaseSync::GetActiveStreamCount() const {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT COUNT(*) FROM detected_streams WHERE is_active = 1"));

  if (statement.Step()) {
    return statement.ColumnInt(0);
  }
  return 0;
}

}  // namespace datasipper
