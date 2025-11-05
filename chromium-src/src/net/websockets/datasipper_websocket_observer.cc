// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/datasipper_websocket_observer.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/websockets/websocket_handshake_response_info.h"

namespace net {

// MessageInfo implementation
DataSipperWebSocketObserver::MessageInfo::MessageInfo() = default;
DataSipperWebSocketObserver::MessageInfo::~MessageInfo() = default;

// ConnectionInfo implementation
DataSipperWebSocketObserver::ConnectionInfo::ConnectionInfo() = default;
DataSipperWebSocketObserver::ConnectionInfo::~ConnectionInfo() = default;

DataSipperWebSocketObserver::DataSipperWebSocketObserver(const GURL& url)
    : url_(url), connection_id_(GenerateConnectionId()) {
  connection_info_ = std::make_unique<ConnectionInfo>();
  connection_info_->connection_id = connection_id_;
  connection_info_->url = url;
  connection_info_->start_time = base::Time::Now();

  DVLOG(1) << "DataSipper WebSocket observer created for: " << url.spec()
           << " (ID: " << connection_id_ << ")";
}

DataSipperWebSocketObserver::~DataSipperWebSocketObserver() {
  if (connection_info_ && connection_info_->end_time.is_null()) {
    connection_info_->end_time = base::Time::Now();
    // TODO: Store final connection info in database
  }

  DVLOG(1) << "DataSipper WebSocket observer destroyed for connection: "
           << connection_id_;
}

void DataSipperWebSocketObserver::OnConnectionEstablished(
    const WebSocketHandshakeResponseInfo* response) {
  DVLOG(1) << "DataSipper: WebSocket connection established - " << url_.spec()
           << " (ID: " << connection_id_ << ")";

  // TODO: Capture handshake response headers and other connection details
  if (response) {
    // Store handshake information
  }
}

void DataSipperWebSocketObserver::OnConnectionFailed(const std::string& error_message) {
  DVLOG(1) << "DataSipper: WebSocket connection failed - " << url_.spec()
           << " (ID: " << connection_id_ << ") Error: " << error_message;

  connection_info_->error_message = error_message;
  connection_info_->end_time = base::Time::Now();
}

void DataSipperWebSocketObserver::OnConnectionClosed(bool was_clean,
                                                   uint16_t code,
                                                   const std::string& reason) {
  DVLOG(1) << "DataSipper: WebSocket connection closed - " << url_.spec()
           << " (ID: " << connection_id_ << ") Clean: " << was_clean
           << " Code: " << code << " Reason: " << reason;

  connection_info_->was_clean_close = was_clean;
  connection_info_->close_code = code;
  connection_info_->close_reason = reason;
  connection_info_->end_time = base::Time::Now();

  // TODO: Store final connection summary in database
}

void DataSipperWebSocketObserver::OnSendFrame(bool fin,
                                            WebSocketFrameHeader::OpCode opcode,
                                            const std::vector<char>& data) {
  MessageInfo message;
  message.connection_id = connection_id_;
  message.timestamp = base::Time::Now();
  message.is_outgoing = true;
  message.opcode = opcode;
  message.fin = fin;
  message.payload_size = data.size();

  // Convert data to string for text frames, or store as hex for binary
  if (opcode == WebSocketFrameHeader::kOpCodeText) {
    message.payload = std::string(data.begin(), data.end());
  } else if (opcode == WebSocketFrameHeader::kOpCodeBinary) {
    // Store binary data as hex string (limited size for display)
    size_t display_size = std::min(data.size(), size_t(1024));
    message.payload = base::HexEncode(data.data(), display_size);
    if (data.size() > display_size) {
      message.payload += "... (truncated)";
    }
  } else {
    message.payload = "[" + OpCodeToString(opcode) + " frame]";
  }

  ProcessMessage(message);
  UpdateConnectionStats(message);

  DVLOG(2) << "DataSipper: WebSocket SEND " << OpCodeToString(opcode)
           << " (" << data.size() << " bytes) - " << connection_id_;
}

void DataSipperWebSocketObserver::OnReceiveFrame(bool fin,
                                               WebSocketFrameHeader::OpCode opcode,
                                               const std::vector<char>& data) {
  MessageInfo message;
  message.connection_id = connection_id_;
  message.timestamp = base::Time::Now();
  message.is_outgoing = false;
  message.opcode = opcode;
  message.fin = fin;
  message.payload_size = data.size();

  // Convert data to string for text frames, or store as hex for binary
  if (opcode == WebSocketFrameHeader::kOpCodeText) {
    message.payload = std::string(data.begin(), data.end());
  } else if (opcode == WebSocketFrameHeader::kOpCodeBinary) {
    // Store binary data as hex string (limited size for display)
    size_t display_size = std::min(data.size(), size_t(1024));
    message.payload = base::HexEncode(data.data(), display_size);
    if (data.size() > display_size) {
      message.payload += "... (truncated)";
    }
  } else {
    message.payload = "[" + OpCodeToString(opcode) + " frame]";
  }

  ProcessMessage(message);
  UpdateConnectionStats(message);

  DVLOG(2) << "DataSipper: WebSocket RECV " << OpCodeToString(opcode)
           << " (" << data.size() << " bytes) - " << connection_id_;
}

std::string DataSipperWebSocketObserver::GenerateConnectionId() {
  return "ws_" + base::NumberToString(base::RandUint64());
}

std::string DataSipperWebSocketObserver::OpCodeToString(WebSocketFrameHeader::OpCode opcode) {
  switch (opcode) {
    case WebSocketFrameHeader::kOpCodeText:
      return "TEXT";
    case WebSocketFrameHeader::kOpCodeBinary:
      return "BINARY";
    case WebSocketFrameHeader::kOpCodeClose:
      return "CLOSE";
    case WebSocketFrameHeader::kOpCodePing:
      return "PING";
    case WebSocketFrameHeader::kOpCodePong:
      return "PONG";
    default:
      return "UNKNOWN";
  }
}

void DataSipperWebSocketObserver::ProcessMessage(const MessageInfo& message) {
  // Send to DataSipper panel for real-time display
  SendToDataSipperPanel(message);

  // Store in database for persistence
  StoreInDatabase(message);
}

void DataSipperWebSocketObserver::SendToDataSipperPanel(const MessageInfo& message) {
  // TODO: Implement IPC to send WebSocket message to DataSipper UI panel
}

void DataSipperWebSocketObserver::StoreInDatabase(const MessageInfo& message) {
  // TODO: Implement database storage for WebSocket messages
}

void DataSipperWebSocketObserver::UpdateConnectionStats(const MessageInfo& message) {
  if (message.is_outgoing) {
    connection_info_->messages_sent++;
    connection_info_->bytes_sent += message.payload_size;
  } else {
    connection_info_->messages_received++;
    connection_info_->bytes_received += message.payload_size;
  }
}

}  // namespace net
