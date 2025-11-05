// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_DATASIPPER_DATABASE_H_
#define COMPONENTS_DATASIPPER_DATASIPPER_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace sql {
class Database;
}  // namespace sql

namespace datasipper {

// Stores HTTP requests/responses and WebSocket traffic in an SQLite database.
// All methods should be called on the same sequence.
class DataSipperDatabase {
 public:
  // HTTP Request/Response record
  struct HttpRecord {
    HttpRecord();
    HttpRecord(const HttpRecord&);
    HttpRecord& operator=(const HttpRecord&);
    ~HttpRecord();

    int64_t id = 0;
    std::string request_id;
    std::string url;
    std::string method;
    int status_code = 0;
    std::string request_headers;
    std::string response_headers;
    std::string request_body;
    std::string response_body;
    base::Time timestamp;
    int64_t duration_ms = 0;
  };

  // WebSocket Connection record
  struct WebSocketConnection {
    WebSocketConnection();
    WebSocketConnection(const WebSocketConnection&);
    WebSocketConnection& operator=(const WebSocketConnection&);
    ~WebSocketConnection();

    int64_t id = 0;
    std::string connection_id;
    std::string url;
    base::Time start_time;
    base::Time end_time;
    bool was_clean_close = false;
    uint16_t close_code = 0;
    std::string close_reason;
    std::string error_message;
    int64_t messages_sent = 0;
    int64_t messages_received = 0;
    int64_t bytes_sent = 0;
    int64_t bytes_received = 0;
  };

  // WebSocket Message record
  struct WebSocketMessage {
    WebSocketMessage();
    WebSocketMessage(const WebSocketMessage&);
    WebSocketMessage& operator=(const WebSocketMessage&);
    ~WebSocketMessage();

    int64_t id = 0;
    std::string connection_id;
    base::Time timestamp;
    bool is_outgoing = false;
    int opcode = 0;  // WebSocketFrameHeader::OpCode
    bool fin = false;
    std::string payload;
    int64_t payload_size = 0;
  };

  DataSipperDatabase();

  DataSipperDatabase(const DataSipperDatabase&) = delete;
  DataSipperDatabase& operator=(const DataSipperDatabase&) = delete;

  ~DataSipperDatabase();

  // Opens an existing database at |path|, or creates a new one if none exists.
  // Returns true on success.
  bool Init(const base::FilePath& path);

  // HTTP Methods
  bool StoreHttpRequest(const HttpRecord& record);
  bool UpdateHttpResponse(const std::string& request_id,
                         int status_code,
                         const std::string& response_headers,
                         const std::string& response_body,
                         int64_t duration_ms);
  std::vector<HttpRecord> GetAllHttpRecords() const;
  std::vector<HttpRecord> GetHttpRecordsByUrl(const std::string& url) const;

  // WebSocket Methods
  bool StoreWebSocketConnection(const WebSocketConnection& connection);
  bool UpdateWebSocketConnection(const std::string& connection_id,
                                 const WebSocketConnection& connection);
  bool StoreWebSocketMessage(const WebSocketMessage& message);
  std::vector<WebSocketConnection> GetAllWebSocketConnections() const;
  std::vector<WebSocketMessage> GetWebSocketMessages(
      const std::string& connection_id) const;

  // Utility Methods
  bool ClearAllData();
  int64_t GetDatabaseSize() const;

  // Access to underlying database (for WorkflowStorage, etc.)
  sql::Database* db() const { return db_.get(); }

 private:
  bool CreateSchema();
  bool CreateHttpTables();
  bool CreateWebSocketTables();

  base::FilePath db_path_;
  std::unique_ptr<sql::Database> db_;
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_DATASIPPER_DATABASE_H_
