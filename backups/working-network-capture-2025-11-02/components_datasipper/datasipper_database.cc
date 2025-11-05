// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/datasipper_database.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace datasipper {

namespace {

const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;

inline constexpr sql::Database::Tag kDatabaseTag{"DataSipper"};

}  // namespace

// HttpRecord
DataSipperDatabase::HttpRecord::HttpRecord() = default;
DataSipperDatabase::HttpRecord::HttpRecord(const HttpRecord&) = default;
DataSipperDatabase::HttpRecord& DataSipperDatabase::HttpRecord::operator=(
    const HttpRecord&) = default;
DataSipperDatabase::HttpRecord::~HttpRecord() = default;

// WebSocketConnection
DataSipperDatabase::WebSocketConnection::WebSocketConnection() = default;
DataSipperDatabase::WebSocketConnection::WebSocketConnection(
    const WebSocketConnection&) = default;
DataSipperDatabase::WebSocketConnection&
DataSipperDatabase::WebSocketConnection::operator=(
    const WebSocketConnection&) = default;
DataSipperDatabase::WebSocketConnection::~WebSocketConnection() = default;

// WebSocketMessage
DataSipperDatabase::WebSocketMessage::WebSocketMessage() = default;
DataSipperDatabase::WebSocketMessage::WebSocketMessage(
    const WebSocketMessage&) = default;
DataSipperDatabase::WebSocketMessage&
DataSipperDatabase::WebSocketMessage::operator=(const WebSocketMessage&) =
    default;
DataSipperDatabase::WebSocketMessage::~WebSocketMessage() = default;

DataSipperDatabase::DataSipperDatabase() = default;

DataSipperDatabase::~DataSipperDatabase() = default;

bool DataSipperDatabase::Init(const base::FilePath& path) {
  DCHECK(!db_);

  db_path_ = path;
  db_ = std::make_unique<sql::Database>(kDatabaseTag);

  // Ensure parent directory exists
  base::FilePath dir = path.DirName();
  if (!base::DirectoryExists(dir)) {
    if (!base::CreateDirectory(dir)) {
      LOG(ERROR) << "Failed to create DataSipper database directory: "
                 << dir.value();
      return false;
    }
  }

  if (!db_->Open(path)) {
    LOG(ERROR) << "Failed to open DataSipper database: " << path.value();
    return false;
  }

  if (!CreateSchema()) {
    LOG(ERROR) << "Failed to create DataSipper database schema";
    db_->Close();
    return false;
  }

  LOG(INFO) << "DataSipper database initialized at: " << path.value();
  return true;
}

bool DataSipperDatabase::CreateSchema() {
  if (!db_)
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  // Create metadata table
  if (!db_->Execute(
          "CREATE TABLE IF NOT EXISTS meta("
          "key TEXT NOT NULL UNIQUE PRIMARY KEY,"
          "value TEXT)")) {
    return false;
  }

  // Set version
  sql::Statement version_stmt(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO meta(key, value) VALUES(?, ?)"));
  version_stmt.BindString(0, "version");
  version_stmt.BindInt(1, kCurrentVersionNumber);
  if (!version_stmt.Run())
    return false;

  version_stmt.Reset(true);
  version_stmt.BindString(0, "last_compatible_version");
  version_stmt.BindInt(1, kCompatibleVersionNumber);
  if (!version_stmt.Run())
    return false;

  if (!CreateHttpTables() || !CreateWebSocketTables()) {
    return false;
  }

  return transaction.Commit();
}

bool DataSipperDatabase::CreateHttpTables() {
  // HTTP requests/responses table
  if (!db_->Execute(
          "CREATE TABLE IF NOT EXISTS http_records("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "request_id TEXT NOT NULL UNIQUE,"
          "url TEXT NOT NULL,"
          "method TEXT NOT NULL,"
          "status_code INTEGER,"
          "request_headers TEXT,"
          "response_headers TEXT,"
          "request_body TEXT,"
          "response_body TEXT,"
          "timestamp INTEGER NOT NULL,"
          "duration_ms INTEGER)")) {
    return false;
  }

  // Create indexes separately
  if (!db_->Execute(
          "CREATE INDEX IF NOT EXISTS idx_http_timestamp "
          "ON http_records(timestamp)")) {
    return false;
  }

  return db_->Execute(
      "CREATE INDEX IF NOT EXISTS idx_http_url "
      "ON http_records(url)");
}

bool DataSipperDatabase::CreateWebSocketTables() {
  // WebSocket connections table
  if (!db_->Execute(
          "CREATE TABLE IF NOT EXISTS websocket_connections("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "connection_id TEXT NOT NULL UNIQUE,"
          "url TEXT NOT NULL,"
          "start_time INTEGER NOT NULL,"
          "end_time INTEGER,"
          "was_clean_close INTEGER DEFAULT 0,"
          "close_code INTEGER,"
          "close_reason TEXT,"
          "error_message TEXT,"
          "messages_sent INTEGER DEFAULT 0,"
          "messages_received INTEGER DEFAULT 0,"
          "bytes_sent INTEGER DEFAULT 0,"
          "bytes_received INTEGER DEFAULT 0)")) {
    return false;
  }

  // Create indexes for connections
  if (!db_->Execute(
          "CREATE INDEX IF NOT EXISTS idx_ws_conn_start_time "
          "ON websocket_connections(start_time)")) {
    return false;
  }

  if (!db_->Execute(
          "CREATE INDEX IF NOT EXISTS idx_ws_conn_url "
          "ON websocket_connections(url)")) {
    return false;
  }

  // WebSocket messages table
  if (!db_->Execute(
          "CREATE TABLE IF NOT EXISTS websocket_messages("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "connection_id TEXT NOT NULL,"
          "timestamp INTEGER NOT NULL,"
          "is_outgoing INTEGER NOT NULL,"
          "opcode INTEGER NOT NULL,"
          "fin INTEGER NOT NULL,"
          "payload TEXT,"
          "payload_size INTEGER NOT NULL,"
          "FOREIGN KEY(connection_id) REFERENCES websocket_connections(connection_id))")) {
    return false;
  }

  // Create indexes for messages
  if (!db_->Execute(
          "CREATE INDEX IF NOT EXISTS idx_ws_msg_connection_id "
          "ON websocket_messages(connection_id)")) {
    return false;
  }

  return db_->Execute(
      "CREATE INDEX IF NOT EXISTS idx_ws_msg_timestamp "
      "ON websocket_messages(timestamp)");
}

// HTTP Methods

bool DataSipperDatabase::StoreHttpRequest(const HttpRecord& record) {
  if (!db_)
    return false;

  sql::Statement stmt(db_->GetUniqueStatement(
      "INSERT INTO http_records("
      "request_id, url, method, request_headers, request_body, timestamp) "
      "VALUES(?, ?, ?, ?, ?, ?)"));

  stmt.BindString(0, record.request_id);
  stmt.BindString(1, record.url);
  stmt.BindString(2, record.method);
  stmt.BindString(3, record.request_headers);
  stmt.BindString(4, record.request_body);
  stmt.BindInt64(5, record.timestamp.InMillisecondsSinceUnixEpoch());

  return stmt.Run();
}

bool DataSipperDatabase::UpdateHttpResponse(const std::string& request_id,
                                            int status_code,
                                            const std::string& response_headers,
                                            const std::string& response_body,
                                            int64_t duration_ms) {
  if (!db_)
    return false;

  sql::Statement stmt(db_->GetUniqueStatement(
      "UPDATE http_records SET "
      "status_code = ?, response_headers = ?, response_body = ?, duration_ms = ? "
      "WHERE request_id = ?"));

  stmt.BindInt(0, status_code);
  stmt.BindString(1, response_headers);
  stmt.BindString(2, response_body);
  stmt.BindInt64(3, duration_ms);
  stmt.BindString(4, request_id);

  return stmt.Run();
}

std::vector<DataSipperDatabase::HttpRecord>
DataSipperDatabase::GetAllHttpRecords() const {
  std::vector<HttpRecord> records;
  if (!db_)
    return records;

  sql::Statement stmt(db_->GetUniqueStatement(
      "SELECT id, request_id, url, method, status_code, "
      "request_headers, response_headers, request_body, response_body, "
      "timestamp, duration_ms FROM http_records ORDER BY timestamp DESC"));

  while (stmt.Step()) {
    HttpRecord record;
    record.id = stmt.ColumnInt64(0);
    record.request_id = stmt.ColumnString(1);
    record.url = stmt.ColumnString(2);
    record.method = stmt.ColumnString(3);
    record.status_code = stmt.ColumnInt(4);
    record.request_headers = stmt.ColumnString(5);
    record.response_headers = stmt.ColumnString(6);
    record.request_body = stmt.ColumnString(7);
    record.response_body = stmt.ColumnString(8);
    record.timestamp = base::Time::FromMillisecondsSinceUnixEpoch(
        stmt.ColumnInt64(9));
    record.duration_ms = stmt.ColumnInt64(10);
    records.push_back(std::move(record));
  }

  return records;
}

std::vector<DataSipperDatabase::HttpRecord>
DataSipperDatabase::GetHttpRecordsByUrl(const std::string& url) const {
  std::vector<HttpRecord> records;
  if (!db_)
    return records;

  sql::Statement stmt(db_->GetUniqueStatement(
      "SELECT id, request_id, url, method, status_code, "
      "request_headers, response_headers, request_body, response_body, "
      "timestamp, duration_ms FROM http_records WHERE url = ? "
      "ORDER BY timestamp DESC"));

  stmt.BindString(0, url);

  while (stmt.Step()) {
    HttpRecord record;
    record.id = stmt.ColumnInt64(0);
    record.request_id = stmt.ColumnString(1);
    record.url = stmt.ColumnString(2);
    record.method = stmt.ColumnString(3);
    record.status_code = stmt.ColumnInt(4);
    record.request_headers = stmt.ColumnString(5);
    record.response_headers = stmt.ColumnString(6);
    record.request_body = stmt.ColumnString(7);
    record.response_body = stmt.ColumnString(8);
    record.timestamp = base::Time::FromMillisecondsSinceUnixEpoch(
        stmt.ColumnInt64(9));
    record.duration_ms = stmt.ColumnInt64(10);
    records.push_back(std::move(record));
  }

  return records;
}

// WebSocket Methods

bool DataSipperDatabase::StoreWebSocketConnection(
    const WebSocketConnection& connection) {
  if (!db_)
    return false;

  sql::Statement stmt(db_->GetUniqueStatement(
      "INSERT INTO websocket_connections("
      "connection_id, url, start_time) VALUES(?, ?, ?)"));

  stmt.BindString(0, connection.connection_id);
  stmt.BindString(1, connection.url);
  stmt.BindInt64(2, connection.start_time.InMillisecondsSinceUnixEpoch());

  return stmt.Run();
}

bool DataSipperDatabase::UpdateWebSocketConnection(
    const std::string& connection_id,
    const WebSocketConnection& connection) {
  if (!db_)
    return false;

  sql::Statement stmt(db_->GetUniqueStatement(
      "UPDATE websocket_connections SET "
      "end_time = ?, was_clean_close = ?, close_code = ?, close_reason = ?, "
      "error_message = ?, messages_sent = ?, messages_received = ?, "
      "bytes_sent = ?, bytes_received = ? "
      "WHERE connection_id = ?"));

  stmt.BindInt64(0, connection.end_time.InMillisecondsSinceUnixEpoch());
  stmt.BindBool(1, connection.was_clean_close);
  stmt.BindInt(2, connection.close_code);
  stmt.BindString(3, connection.close_reason);
  stmt.BindString(4, connection.error_message);
  stmt.BindInt64(5, connection.messages_sent);
  stmt.BindInt64(6, connection.messages_received);
  stmt.BindInt64(7, connection.bytes_sent);
  stmt.BindInt64(8, connection.bytes_received);
  stmt.BindString(9, connection_id);

  return stmt.Run();
}

bool DataSipperDatabase::StoreWebSocketMessage(
    const WebSocketMessage& message) {
  if (!db_)
    return false;

  sql::Statement stmt(db_->GetUniqueStatement(
      "INSERT INTO websocket_messages("
      "connection_id, timestamp, is_outgoing, opcode, fin, payload, payload_size) "
      "VALUES(?, ?, ?, ?, ?, ?, ?)"));

  stmt.BindString(0, message.connection_id);
  stmt.BindInt64(1, message.timestamp.InMillisecondsSinceUnixEpoch());
  stmt.BindBool(2, message.is_outgoing);
  stmt.BindInt(3, message.opcode);
  stmt.BindBool(4, message.fin);
  stmt.BindString(5, message.payload);
  stmt.BindInt64(6, message.payload_size);

  return stmt.Run();
}

std::vector<DataSipperDatabase::WebSocketConnection>
DataSipperDatabase::GetAllWebSocketConnections() const {
  std::vector<WebSocketConnection> connections;
  if (!db_)
    return connections;

  sql::Statement stmt(db_->GetUniqueStatement(
      "SELECT id, connection_id, url, start_time, end_time, "
      "was_clean_close, close_code, close_reason, error_message, "
      "messages_sent, messages_received, bytes_sent, bytes_received "
      "FROM websocket_connections ORDER BY start_time DESC"));

  while (stmt.Step()) {
    WebSocketConnection conn;
    conn.id = stmt.ColumnInt64(0);
    conn.connection_id = stmt.ColumnString(1);
    conn.url = stmt.ColumnString(2);
    conn.start_time = base::Time::FromMillisecondsSinceUnixEpoch(
        stmt.ColumnInt64(3));
    conn.end_time = base::Time::FromMillisecondsSinceUnixEpoch(
        stmt.ColumnInt64(4));
    conn.was_clean_close = stmt.ColumnBool(5);
    conn.close_code = static_cast<uint16_t>(stmt.ColumnInt(6));
    conn.close_reason = stmt.ColumnString(7);
    conn.error_message = stmt.ColumnString(8);
    conn.messages_sent = stmt.ColumnInt64(9);
    conn.messages_received = stmt.ColumnInt64(10);
    conn.bytes_sent = stmt.ColumnInt64(11);
    conn.bytes_received = stmt.ColumnInt64(12);
    connections.push_back(std::move(conn));
  }

  return connections;
}

std::vector<DataSipperDatabase::WebSocketMessage>
DataSipperDatabase::GetWebSocketMessages(
    const std::string& connection_id) const {
  std::vector<WebSocketMessage> messages;
  if (!db_)
    return messages;

  sql::Statement stmt(db_->GetUniqueStatement(
      "SELECT id, connection_id, timestamp, is_outgoing, opcode, fin, "
      "payload, payload_size FROM websocket_messages "
      "WHERE connection_id = ? ORDER BY timestamp ASC"));

  stmt.BindString(0, connection_id);

  while (stmt.Step()) {
    WebSocketMessage msg;
    msg.id = stmt.ColumnInt64(0);
    msg.connection_id = stmt.ColumnString(1);
    msg.timestamp = base::Time::FromMillisecondsSinceUnixEpoch(
        stmt.ColumnInt64(2));
    msg.is_outgoing = stmt.ColumnBool(3);
    msg.opcode = stmt.ColumnInt(4);
    msg.fin = stmt.ColumnBool(5);
    msg.payload = stmt.ColumnString(6);
    msg.payload_size = stmt.ColumnInt64(7);
    messages.push_back(std::move(msg));
  }

  return messages;
}

// Utility Methods

bool DataSipperDatabase::ClearAllData() {
  if (!db_)
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  if (!db_->Execute("DELETE FROM http_records"))
    return false;

  if (!db_->Execute("DELETE FROM websocket_messages"))
    return false;

  if (!db_->Execute("DELETE FROM websocket_connections"))
    return false;

  return transaction.Commit();
}

int64_t DataSipperDatabase::GetDatabaseSize() const {
  if (!db_ || db_path_.empty())
    return 0;

  std::optional<int64_t> size = base::GetFileSize(db_path_);
  return size.value_or(0);
}

}  // namespace datasipper
