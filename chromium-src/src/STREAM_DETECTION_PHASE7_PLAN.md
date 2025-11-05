# DataSipper Stream Detection - Phase 7 Plan

**Phase:** Database & Mojo Integration
**Effort:** 4-5 days
**Priority:** High (enables persistence and IPC)
**Status:** Not started
**Depends On:** Phase 6 (Stream Registry)

## Overview

Phase 7 adds database persistence for streams and creates Mojo APIs for cross-process communication. This enables stream data to persist across browser sessions and provides a clean IPC layer for the WebUI to access stream information.

## Goals

1. **Database Schema**: Extend database to store streams and events
2. **Stream Persistence**: Save/load streams from database
3. **Event History**: Store stream events for historical analysis
4. **Mojo API**: IPC methods for WebUI access
5. **Subscription API**: Mojo methods for subscribing to streams
6. **Auto-Sync**: Automatically sync registry to database

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                      Browser Process                          │
│                                                               │
│  ┌────────────────────────────────────────────────────────┐  │
│  │              DataSipperService                          │  │
│  │                                                         │  │
│  │  ┌──────────────┐         ┌──────────────────────┐    │  │
│  │  │StreamRegistry│◄────────┤ StreamDatabaseSync   │    │  │
│  │  │ (in-memory)  │ restore │ (new)                │    │  │
│  │  └──────┬───────┘         └──────────┬───────────┘    │  │
│  │         │                            │                 │  │
│  │         │ OnStreamDetected           │ Save/Load       │  │
│  │         ▼                            ▼                 │  │
│  │  ┌──────────────────────────────────────────────┐     │  │
│  │  │        DataSipperDatabase                     │     │  │
│  │  │                                               │     │  │
│  │  │  Tables:                                      │     │  │
│  │  │  - detected_streams                           │     │  │
│  │  │  - stream_events                              │     │  │
│  │  │  - stream_subscriptions                       │     │  │
│  │  └──────────────────────────────────────────────┘     │  │
│  │                                                         │  │
│  │  ┌──────────────────────────────────────────────────┐ │  │
│  │  │       DataSipperPageHandler (Mojo endpoint)      │ │  │
│  │  │                                                  │ │  │
│  │  │  GetDetectedStreams() → vector<StreamInfo>     │ │  │
│  │  │  GetStreamEvents(id) → vector<EventInfo>       │ │  │
│  │  │  SubscribeToStream(id, callback)               │ │  │
│  │  │  UnsubscribeFromStream(id)                     │ │  │
│  │  │  ExportStreamDefinition(id) → WorkflowConfig   │ │  │
│  │  └──────────────────────────────────────────────────┘ │  │
│  │                      │                                  │  │
│  └──────────────────────┼──────────────────────────────────┘  │
│                         │ Mojo IPC                            │
│                         ▼                                     │
│  ┌────────────────────────────────────────────────────────┐  │
│  │                  Renderer Process                       │  │
│  │                                                         │  │
│  │  ┌──────────────────────────────────────────────────┐  │  │
│  │  │         datasipper_ui.ts (WebUI)                  │  │  │
│  │  │                                                   │  │  │
│  │  │  pageHandler.getDetectedStreams()                │  │  │
│  │  │  pageHandler.subscribeToStream(id)               │  │  │
│  │  │  onStreamEventReceived(event)                    │  │  │
│  │  └──────────────────────────────────────────────────┘  │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

## Database Schema

### Table: detected_streams
```sql
CREATE TABLE detected_streams (
  stream_id TEXT PRIMARY KEY,
  label TEXT NOT NULL,
  stream_type INTEGER NOT NULL,  -- StreamType enum
  source_pattern TEXT NOT NULL,
  url TEXT,

  -- Timing information
  first_seen INTEGER NOT NULL,   -- Unix timestamp ms
  last_seen INTEGER NOT NULL,
  avg_interval_ms REAL,
  std_deviation REAL,
  is_regular INTEGER,            -- Boolean

  -- Stream state
  is_active INTEGER NOT NULL,    -- Boolean
  event_count INTEGER DEFAULT 0,

  -- Data information
  data_type INTEGER,             -- DataType enum
  sample_data TEXT,              -- JSON sample

  -- Metadata
  registered_at INTEGER NOT NULL,
  last_update INTEGER NOT NULL,

  UNIQUE(source_pattern, stream_type)
);

CREATE INDEX idx_streams_type ON detected_streams(stream_type);
CREATE INDEX idx_streams_active ON detected_streams(is_active);
CREATE INDEX idx_streams_pattern ON detected_streams(source_pattern);
CREATE INDEX idx_streams_url ON detected_streams(url);
```

### Table: stream_events
```sql
CREATE TABLE stream_events (
  event_id INTEGER PRIMARY KEY AUTOINCREMENT,
  stream_id TEXT NOT NULL,
  timestamp INTEGER NOT NULL,    -- Unix timestamp ms
  event_type TEXT NOT NULL,      -- "update", "error", "close"
  data TEXT,                     -- JSON payload

  FOREIGN KEY(stream_id) REFERENCES detected_streams(stream_id)
    ON DELETE CASCADE
);

CREATE INDEX idx_events_stream ON stream_events(stream_id);
CREATE INDEX idx_events_timestamp ON stream_events(timestamp);
CREATE INDEX idx_events_type ON stream_events(stream_id, event_type);
```

### Table: stream_subscriptions
```sql
CREATE TABLE stream_subscriptions (
  subscription_id TEXT PRIMARY KEY,
  stream_id TEXT NOT NULL,
  subscriber_type TEXT NOT NULL,  -- "workflow", "export", "manual"
  subscriber_data TEXT,           -- JSON (workflow_id, export_config, etc.)

  created_at INTEGER NOT NULL,
  is_active INTEGER NOT NULL,
  events_received INTEGER DEFAULT 0,

  FOREIGN KEY(stream_id) REFERENCES detected_streams(stream_id)
    ON DELETE CASCADE
);

CREATE INDEX idx_subscriptions_stream ON stream_subscriptions(stream_id);
CREATE INDEX idx_subscriptions_active ON stream_subscriptions(is_active);
```

## Database Integration

### StreamDatabaseSync Class

**Location:** `components/datasipper/streaming/stream_database_sync.h`

```cpp
#ifndef COMPONENTS_DATASIPPER_STREAMING_STREAM_DATABASE_SYNC_H_
#define COMPONENTS_DATASIPPER_STREAMING_STREAM_DATABASE_SYNC_H_

#include "base/memory/raw_ptr.h"
#include "components/datasipper/streaming/stream_registry.h"

namespace sql {
class Database;
}

namespace datasipper {

class StreamRegistry;
struct StreamDefinition;
struct StreamEvent;

// Synchronizes StreamRegistry with database storage
class StreamDatabaseSync {
 public:
  explicit StreamDatabaseSync(sql::Database* db,
                              StreamRegistry* registry);
  ~StreamDatabaseSync();

  StreamDatabaseSync(const StreamDatabaseSync&) = delete;
  StreamDatabaseSync& operator=(const StreamDatabaseSync&) = delete;

  // Initialize database tables
  bool Initialize();

  // Load streams from database into registry
  bool LoadStreamsFromDatabase();

  // Save operations (called automatically)
  bool SaveStream(const StreamDefinition& stream);
  bool UpdateStream(const std::string& stream_id,
                   const StreamDefinition& stream);
  bool DeleteStream(const std::string& stream_id);

  // Event storage
  bool SaveEvent(const StreamEvent& event);
  std::vector<StreamEvent> GetStreamEvents(
      const std::string& stream_id,
      int limit = 100) const;
  std::vector<StreamEvent> GetRecentEvents(
      const std::string& stream_id,
      base::Time since) const;

  // Subscription persistence
  bool SaveSubscription(const std::string& stream_id,
                       const std::string& subscriber_type,
                       const std::string& subscriber_data);
  bool LoadSubscriptions();

  // Cleanup
  bool DeleteOldEvents(base::TimeDelta older_than);
  bool DeleteInactiveStreams(base::TimeDelta inactive_for);

  // Statistics
  int GetStreamCount() const;
  int GetEventCount() const;

 private:
  bool CreateTables();
  bool MigrateTables();

  raw_ptr<sql::Database> db_;
  raw_ptr<StreamRegistry> registry_;
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_STREAMING_STREAM_DATABASE_SYNC_H_
```

### Key Implementation Methods

#### Initialize()
```cpp
bool StreamDatabaseSync::Initialize() {
  if (!db_ || !registry_) {
    return false;
  }

  // Create tables if they don't exist
  if (!CreateTables()) {
    LOG(ERROR) << "Failed to create stream tables";
    return false;
  }

  // Load existing streams into registry
  if (!LoadStreamsFromDatabase()) {
    LOG(WARNING) << "Failed to load streams from database";
    // Non-fatal, continue
  }

  // Restore subscriptions
  if (!LoadSubscriptions()) {
    LOG(WARNING) << "Failed to load subscriptions";
    // Non-fatal
  }

  LOG(INFO) << "✅ StreamDatabaseSync: Initialized";
  return true;
}
```

#### SaveStream()
```cpp
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
```

#### SaveEvent()
```cpp
bool StreamDatabaseSync::SaveEvent(const StreamEvent& event) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO stream_events "
      "(stream_id, timestamp, event_type, data) "
      "VALUES (?, ?, ?, ?)"));

  statement.BindString(0, event.stream_id);
  statement.BindInt64(1, event.timestamp.InMillisecondsSinceUnixEpoch());
  statement.BindString(2, event.event_type);

  // Serialize data to JSON
  std::string json_data;
  base::JSONWriter::Write(event.data, &json_data);
  statement.BindString(3, json_data);

  return statement.Run();
}
```

#### LoadStreamsFromDatabase()
```cpp
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
    stream.last_seen = base::Time::FromMillisecondsSinceUnixEpoch(
        statement.ColumnInt64(6));

    stream.timing.avg_interval_ms = statement.ColumnDouble(7);
    stream.timing.std_deviation = statement.ColumnDouble(8);
    stream.timing.is_regular = statement.ColumnBool(9);
    stream.is_active = statement.ColumnBool(10);
    stream.event_count = statement.ColumnInt(11);
    stream.data_type = static_cast<DataType>(statement.ColumnInt(12));

    // Register in registry
    registry_->RegisterStream(stream);
    loaded++;
  }

  LOG(INFO) << "✅ StreamDatabaseSync: Loaded " << loaded << " streams";
  return statement.Succeeded();
}
```

## Mojo API Definition

### Update datasipper.mojom

**Location:** `chrome/browser/ui/webui/side_panel/datasipper/datasipper.mojom`

```cpp
// Add to existing datasipper.mojom file

// Stream type enum
enum StreamType {
  DOM_MUTATION = 0,
  NETWORK_PATTERN = 1,
  WEBSOCKET_STREAM = 2,
};

// Data type enum
enum DataType {
  UNKNOWN = 0,
  JSON = 1,
  XML = 2,
  CSV = 3,
  TEXT = 4,
  BINARY = 5,
};

// Timing information for streams
struct StreamTimingInfo {
  double avg_interval_ms;
  double std_deviation;
  bool is_regular;
};

// Stream information for UI
struct StreamInfo {
  string stream_id;
  string label;
  StreamType type;
  string source_pattern;
  string url;

  int64 first_seen;  // Unix timestamp ms
  int64 last_seen;
  StreamTimingInfo timing;

  bool is_active;
  int32 event_count;
  DataType data_type;
};

// Individual stream event
struct StreamEventInfo {
  string stream_id;
  int64 timestamp;  // Unix timestamp ms
  string event_type;
  string data;  // JSON string
};

// Stream subscription handle
struct StreamSubscription {
  string subscription_id;
  string stream_id;
};

// Page interface receives stream updates
interface Page {
  // ... existing methods ...

  // Stream notifications
  OnStreamDetected(StreamInfo stream);
  OnStreamUpdated(StreamInfo stream);
  OnStreamEvent(StreamEventInfo event);
  OnStreamDeactivated(string stream_id);
};

// PageHandler interface - called by WebUI
interface PageHandler {
  // ... existing methods ...

  // Stream management
  GetDetectedStreams() => (array<StreamInfo> streams);
  GetStreamDetails(string stream_id) => (StreamInfo? stream);
  GetStreamEvents(string stream_id, int32 limit)
      => (array<StreamEventInfo> events);

  // Stream subscriptions
  SubscribeToStream(string stream_id) => (StreamSubscription? subscription);
  UnsubscribeFromStream(string subscription_id) => (bool success);

  // Stream export
  ExportStreamAsWorkflow(string stream_id) => (WorkflowInfo? workflow);

  // Stream control
  SetStreamActive(string stream_id, bool active) => (bool success);
  DeleteStream(string stream_id) => (bool success);
};
```

## PageHandler Implementation

### Update DataSipperPageHandler

**Location:** `chrome/browser/ui/webui/side_panel/datasipper/datasipper_page_handler.h`

```cpp
class DataSipperPageHandler : public side_panel::mojom::PageHandler {
 public:
  // ... existing methods ...

  // Stream management (Mojo interface)
  void GetDetectedStreams(GetDetectedStreamsCallback callback) override;
  void GetStreamDetails(const std::string& stream_id,
                       GetStreamDetailsCallback callback) override;
  void GetStreamEvents(const std::string& stream_id,
                      int32_t limit,
                      GetStreamEventsCallback callback) override;

  // Stream subscriptions
  void SubscribeToStream(const std::string& stream_id,
                        SubscribeToStreamCallback callback) override;
  void UnsubscribeFromStream(const std::string& subscription_id,
                            UnsubscribeFromStreamCallback callback) override;

  // Stream export
  void ExportStreamAsWorkflow(const std::string& stream_id,
                             ExportStreamAsWorkflowCallback callback) override;

  // Stream control
  void SetStreamActive(const std::string& stream_id,
                      bool active,
                      SetStreamActiveCallback callback) override;
  void DeleteStream(const std::string& stream_id,
                   DeleteStreamCallback callback) override;

 private:
  // Stream event callback (from registry subscription)
  void OnStreamEventReceived(const datasipper::StreamEvent& event);

  // Active stream subscriptions
  std::map<std::string, std::string> active_subscriptions_;
};
```

### Implementation Examples

#### GetDetectedStreams()
```cpp
void DataSipperPageHandler::GetDetectedStreams(
    GetDetectedStreamsCallback callback) {

  std::vector<side_panel::mojom::StreamInfoPtr> result;

  if (!service_) {
    std::move(callback).Run(std::move(result));
    return;
  }

  auto* registry = service_->stream_registry();
  if (!registry) {
    std::move(callback).Run(std::move(result));
    return;
  }

  // Get all streams from registry
  std::vector<datasipper::StreamDefinition> streams =
      registry->GetAllStreams();

  // Convert to Mojo
  for (const auto& stream : streams) {
    auto stream_info = side_panel::mojom::StreamInfo::New();

    stream_info->stream_id = stream.stream_id;
    stream_info->label = stream.label;
    stream_info->type = ConvertStreamType(stream.type);
    stream_info->source_pattern = stream.source_pattern;
    stream_info->url = stream.url;
    stream_info->first_seen = stream.first_seen.InMillisecondsSinceUnixEpoch();
    stream_info->last_seen = stream.last_seen.InMillisecondsSinceUnixEpoch();

    auto timing = side_panel::mojom::StreamTimingInfo::New();
    timing->avg_interval_ms = stream.timing.avg_interval_ms;
    timing->std_deviation = stream.timing.std_deviation;
    timing->is_regular = stream.timing.is_regular;
    stream_info->timing = std::move(timing);

    stream_info->is_active = stream.is_active;
    stream_info->event_count = stream.event_count;
    stream_info->data_type = ConvertDataType(stream.data_type);

    result.push_back(std::move(stream_info));
  }

  LOG(INFO) << "📊 PageHandler: Returning " << result.size() << " streams";
  std::move(callback).Run(std::move(result));
}
```

#### SubscribeToStream()
```cpp
void DataSipperPageHandler::SubscribeToStream(
    const std::string& stream_id,
    SubscribeToStreamCallback callback) {

  if (!service_ || !service_->stream_registry()) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto* registry = service_->stream_registry();

  // Create subscription
  std::string subscription_id = registry->Subscribe(
      stream_id,
      base::BindRepeating(&DataSipperPageHandler::OnStreamEventReceived,
                         weak_factory_.GetWeakPtr()));

  if (subscription_id.empty()) {
    LOG(ERROR) << "Failed to subscribe to stream: " << stream_id;
    std::move(callback).Run(nullptr);
    return;
  }

  // Track subscription
  active_subscriptions_[subscription_id] = stream_id;

  // Return subscription info
  auto subscription = side_panel::mojom::StreamSubscription::New();
  subscription->subscription_id = subscription_id;
  subscription->stream_id = stream_id;

  LOG(INFO) << "✅ PageHandler: Subscribed to " << stream_id;
  std::move(callback).Run(std::move(subscription));
}
```

#### OnStreamEventReceived()
```cpp
void DataSipperPageHandler::OnStreamEventReceived(
    const datasipper::StreamEvent& event) {

  if (!page_) {
    return;
  }

  // Convert to Mojo
  auto event_info = side_panel::mojom::StreamEventInfo::New();
  event_info->stream_id = event.stream_id;
  event_info->timestamp = event.timestamp.InMillisecondsSinceUnixEpoch();
  event_info->event_type = event.event_type;

  // Serialize data to JSON string
  std::string json_data;
  base::JSONWriter::Write(event.data, &json_data);
  event_info->data = json_data;

  // Send to WebUI
  page_->OnStreamEvent(std::move(event_info));

  DVLOG(2) << "📡 PageHandler: Forwarded event to WebUI";
}
```

## Integration with DataSipperService

### Update DataSipperService

**In datasipper_service.h:**
```cpp
class DataSipperService {
 public:
  // ... existing ...

  StreamDatabaseSync* stream_db_sync() const {
    return stream_db_sync_.get();
  }

 private:
  std::unique_ptr<StreamDatabaseSync> stream_db_sync_;
};
```

**In datasipper_service.cc InitializeStorageComponents():**
```cpp
bool DataSipperService::InitializeStorageComponents() {
  // Initialize stream registry
  stream_registry_ = std::make_unique<StreamRegistry>();
  LOG(INFO) << "✅ DataSipper: Stream registry initialized";

  // Initialize database sync
  if (database_) {
    stream_db_sync_ = std::make_unique<StreamDatabaseSync>(
        database_->db(), stream_registry_.get());

    if (!stream_db_sync_->Initialize()) {
      LOG(ERROR) << "Failed to initialize stream database sync";
      stream_db_sync_.reset();
      // Non-fatal, continue without persistence
    } else {
      LOG(INFO) << "✅ DataSipper: Stream database sync initialized";
    }
  }

  // ... rest of initialization ...
}
```

**In OnStreamDetected():**
```cpp
void DataSipperService::OnStreamDetected(
    content::WebContents* web_contents,
    const StreamDefinition& stream) {

  // ... existing logging ...

  // Register in registry
  if (stream_registry_) {
    std::string stream_id = stream_registry_->RegisterStream(stream);

    // Save to database
    if (stream_db_sync_) {
      stream_db_sync_->SaveStream(stream);
    }

    LOG(INFO) << "📊 Stream registered: " << stream_id;
  }
}
```

**Subscribe registry to save events:**
```cpp
// In InitializeStorageComponents(), after registry creation

// Subscribe to all streams to save events
if (stream_registry_ && stream_db_sync_) {
  // This would need to be done for each stream as they're registered
  // Or add a global event listener to StreamRegistry
}
```

## BUILD.gn Updates

```python
# In components/datasipper/BUILD.gn

# Streaming detection
"streaming/dom_stream_tracker.cc",
"streaming/dom_stream_tracker.h",
"streaming/network_stream_analyzer.cc",
"streaming/network_stream_analyzer.h",
"streaming/stream_database_sync.cc",   # NEW
"streaming/stream_database_sync.h",     # NEW
"streaming/stream_registry.cc",
"streaming/stream_registry.h",
"streaming/stream_types.cc",
"streaming/stream_types.h",
```

## Testing Strategy

### Database Tests
**Location:** `components/datasipper/streaming/stream_database_sync_unittest.cc`

```cpp
TEST_F(StreamDatabaseSyncTest, SaveAndLoadStream) {
  // Test stream persistence
}

TEST_F(StreamDatabaseSyncTest, SaveEvent) {
  // Test event storage
}

TEST_F(StreamDatabaseSyncTest, GetStreamEvents) {
  // Test event retrieval
}

TEST_F(StreamDatabaseSyncTest, DeleteOldEvents) {
  // Test cleanup
}
```

### Mojo Tests
**Location:** `chrome/browser/ui/webui/side_panel/datasipper/datasipper_page_handler_unittest.cc`

```cpp
TEST_F(DataSipperPageHandlerTest, GetDetectedStreams) {
  // Test Mojo stream retrieval
}

TEST_F(DataSipperPageHandlerTest, SubscribeToStream) {
  // Test Mojo subscription
}

TEST_F(DataSipperPageHandlerTest, OnStreamEventReceived) {
  // Test event forwarding to WebUI
}
```

## Success Criteria

Phase 7 complete when:
- ✅ Database schema created and migrated
- ✅ StreamDatabaseSync implemented (~400 lines)
- ✅ Streams persist across browser restarts
- ✅ Events stored in database
- ✅ Mojo API defined in .mojom
- ✅ PageHandler methods implemented
- ✅ WebUI can retrieve streams
- ✅ WebUI can subscribe to events
- ✅ Unit tests passing
- ✅ Browser tests passing
- ✅ Builds without warnings

## Files to Create/Modify

**New Files:**
- `components/datasipper/streaming/stream_database_sync.h` (~150 lines)
- `components/datasipper/streaming/stream_database_sync.cc` (~400 lines)
- `components/datasipper/streaming/stream_database_sync_unittest.cc` (~300 lines)

**Modified Files:**
- `chrome/browser/ui/webui/side_panel/datasipper/datasipper.mojom` (+80 lines)
- `chrome/browser/ui/webui/side_panel/datasipper/datasipper_page_handler.h` (+30 lines)
- `chrome/browser/ui/webui/side_panel/datasipper/datasipper_page_handler.cc` (+200 lines)
- `components/datasipper/datasipper_service.h` (+5 lines)
- `components/datasipper/datasipper_service.cc` (+30 lines)
- `components/datasipper/BUILD.gn` (+2 lines)

**Total:** ~1,197 lines

## Database Migration

If database already exists, add migration in schema_manager.cc:

```cpp
bool SchemaManager::MigrateToVersion8() {
  // Add stream tables
  sql::Statement create_streams(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "CREATE TABLE detected_streams (...)"));

  if (!create_streams.Run()) {
    return false;
  }

  // Add event table
  sql::Statement create_events(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "CREATE TABLE stream_events (...)"));

  return create_events.Run();
}
```

## Notes

- Database sync is **automatic** after Phase 7
- Stream events have configurable retention (default: 7 days)
- Mojo API enables real-time WebUI updates
- Subscription lifecycle tied to PageHandler lifetime
- Event history enables debugging and analysis
- Export functionality generates workflow configs
