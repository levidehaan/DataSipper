// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_DATASIPPER_WEBSOCKET_OBSERVER_H_
#define NET_WEBSOCKETS_DATASIPPER_WEBSOCKET_OBSERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/websockets/websocket_frame.h"
#include "url/gurl.h"

namespace net {
struct WebSocketHandshakeResponseInfo;
}

namespace net {

// Observer class that captures WebSocket messages and connection events
// for the DataSipper monitoring panel.
class DataSipperWebSocketObserver {
 public:
  explicit DataSipperWebSocketObserver(const GURL& url);
  ~DataSipperWebSocketObserver();

  DataSipperWebSocketObserver(const DataSipperWebSocketObserver&) = delete;
  DataSipperWebSocketObserver& operator=(const DataSipperWebSocketObserver&) = delete;

  // WebSocket event callbacks
  void OnConnectionEstablished(const WebSocketHandshakeResponseInfo* response);
  void OnConnectionFailed(const std::string& error_message);
  void OnConnectionClosed(bool was_clean, uint16_t code, const std::string& reason);

  void OnSendFrame(bool fin,
                  WebSocketFrameHeader::OpCode opcode,
                  const std::vector<char>& data);
  void OnReceiveFrame(bool fin,
                     WebSocketFrameHeader::OpCode opcode,
                     const std::vector<char>& data);

 private:
  struct MessageInfo {
    MessageInfo();
    ~MessageInfo();

    std::string connection_id;
    base::Time timestamp;
    bool is_outgoing;
    WebSocketFrameHeader::OpCode opcode;
    std::string payload;
    bool fin;
    size_t payload_size;
  };

  struct ConnectionInfo {
    ConnectionInfo();
    ~ConnectionInfo();

    std::string connection_id;
    GURL url;
    base::Time start_time;
    base::Time end_time;
    bool was_clean_close = false;
    uint16_t close_code = 0;
    std::string close_reason;
    std::string error_message;
    size_t messages_sent = 0;
    size_t messages_received = 0;
    size_t bytes_sent = 0;
    size_t bytes_received = 0;
  };

  // Helper methods
  std::string GenerateConnectionId();
  std::string OpCodeToString(WebSocketFrameHeader::OpCode opcode);
  void ProcessMessage(const MessageInfo& message);
  void SendToDataSipperPanel(const MessageInfo& message);
  void StoreInDatabase(const MessageInfo& message);
  void UpdateConnectionStats(const MessageInfo& message);

  GURL url_;
  std::string connection_id_;
  std::unique_ptr<ConnectionInfo> connection_info_;
  base::WeakPtrFactory<DataSipperWebSocketObserver> weak_factory_{this};
};

}  // namespace net

#endif  // NET_WEBSOCKETS_DATASIPPER_WEBSOCKET_OBSERVER_H_
