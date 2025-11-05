// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/database/datasipper_database_schema.h"

#include "sql/database.h"
#include "sql/statement.h"

namespace datasipper {
namespace schema {

// Network events table stores HTTP/HTTPS requests and responses
const char kCreateNetworkEventsTableSql[] = R"(
  CREATE TABLE IF NOT EXISTS network_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    connection_id TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    event_type INTEGER NOT NULL,  -- 0=request, 1=response
    url TEXT NOT NULL,
    method TEXT,
    status_code INTEGER,
    request_headers TEXT,
    response_headers TEXT,
    request_body BLOB,
    response_body BLOB,
    request_size INTEGER DEFAULT 0,
    response_size INTEGER DEFAULT 0,
    duration_ms INTEGER DEFAULT 0,
    error_code INTEGER DEFAULT 0
  )
)";

// WebSocket messages table stores individual WebSocket frames
const char kCreateWebSocketMessagesTableSql[] = R"(
  CREATE TABLE IF NOT EXISTS websocket_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    connection_id TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    direction INTEGER NOT NULL,  -- 0=outgoing, 1=incoming
    opcode INTEGER NOT NULL,     -- WebSocket frame opcode
    payload TEXT,                -- Message payload (text or hex for binary)
    payload_size INTEGER NOT NULL,
    is_final INTEGER NOT NULL DEFAULT 1,  -- FIN bit
    FOREIGN KEY (connection_id) REFERENCES connections(connection_id)
  )
)";

// Connections table stores WebSocket connection metadata
const char kCreateConnectionsTableSql[] = R"(
  CREATE TABLE IF NOT EXISTS connections (
    connection_id TEXT PRIMARY KEY,
    url TEXT NOT NULL,
    start_time INTEGER NOT NULL,
    end_time INTEGER,
    close_code INTEGER,
    close_reason TEXT,
    error_message TEXT,
    messages_sent INTEGER DEFAULT 0,
    messages_received INTEGER DEFAULT 0,
    bytes_sent INTEGER DEFAULT 0,
    bytes_received INTEGER DEFAULT 0,
    was_clean_close INTEGER DEFAULT 0
  )
)";

// Configuration table stores DataSipper settings
const char kCreateConfigurationTableSql[] = R"(
  CREATE TABLE IF NOT EXISTS configuration (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_time INTEGER NOT NULL
  )
)";

// Indexes for performance
const char kCreateNetworkEventsTimestampIndexSql[] = R"(
  CREATE INDEX IF NOT EXISTS idx_network_events_timestamp
  ON network_events(timestamp)
)";

const char kCreateNetworkEventsUrlIndexSql[] = R"(
  CREATE INDEX IF NOT EXISTS idx_network_events_url
  ON network_events(url)
)";

const char kCreateWebSocketMessagesTimestampIndexSql[] = R"(
  CREATE INDEX IF NOT EXISTS idx_websocket_messages_timestamp
  ON websocket_messages(timestamp)
)";

const char kCreateWebSocketMessagesConnectionIndexSql[] = R"(
  CREATE INDEX IF NOT EXISTS idx_websocket_messages_connection
  ON websocket_messages(connection_id, timestamp)
)";

const char kCreateConnectionsUrlIndexSql[] = R"(
  CREATE INDEX IF NOT EXISTS idx_connections_url
  ON connections(url)
)";

// Insert statements
const char kInsertNetworkEventSql[] = R"(
  INSERT INTO network_events (
    connection_id, timestamp, event_type, url, method, status_code,
    request_headers, response_headers, request_body, response_body,
    request_size, response_size, duration_ms, error_code
  ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
)";

const char kInsertWebSocketMessageSql[] = R"(
  INSERT INTO websocket_messages (
    connection_id, timestamp, direction, opcode, payload, payload_size, is_final
  ) VALUES (?, ?, ?, ?, ?, ?, ?)
)";

const char kInsertConnectionSql[] = R"(
  INSERT OR REPLACE INTO connections (
    connection_id, url, start_time, end_time, close_code, close_reason,
    error_message, messages_sent, messages_received, bytes_sent, bytes_received,
    was_clean_close
  ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
)";

const char kUpdateConnectionSql[] = R"(
  UPDATE connections SET
    end_time = ?, close_code = ?, close_reason = ?, error_message = ?,
    messages_sent = ?, messages_received = ?, bytes_sent = ?, bytes_received = ?,
    was_clean_close = ?
  WHERE connection_id = ?
)";

bool CreateAllTables(sql::Database* db) {
  if (!db->is_open()) {
    return false;
  }

  // Create all tables
  if (!db->Execute(kCreateNetworkEventsTableSql)) {
    DLOG(ERROR) << "Failed to create network_events table";
    return false;
  }

  if (!db->Execute(kCreateWebSocketMessagesTableSql)) {
    DLOG(ERROR) << "Failed to create websocket_messages table";
    return false;
  }

  if (!db->Execute(kCreateConnectionsTableSql)) {
    DLOG(ERROR) << "Failed to create connections table";
    return false;
  }

  if (!db->Execute(kCreateConfigurationTableSql)) {
    DLOG(ERROR) << "Failed to create configuration table";
    return false;
  }

  return true;
}

bool CreateAllIndexes(sql::Database* db) {
  if (!db->is_open()) {
    return false;
  }

  // Create all indexes
  const char* index_statements[] = {
    kCreateNetworkEventsTimestampIndexSql,
    kCreateNetworkEventsUrlIndexSql,
    kCreateWebSocketMessagesTimestampIndexSql,
    kCreateWebSocketMessagesConnectionIndexSql,
    kCreateConnectionsUrlIndexSql,
  };

  for (const char* statement : index_statements) {
    if (!db->Execute(statement)) {
      DLOG(ERROR) << "Failed to create index";
      return false;
    }
  }

  return true;
}

bool UpgradeSchema(sql::Database* db, int from_version, int to_version) {
  // Currently no schema upgrades needed (version 1 is initial)
  // When we need to upgrade schemas, implement migration logic here
  return from_version == to_version;
}

}  // namespace schema
}  // namespace datasipper
