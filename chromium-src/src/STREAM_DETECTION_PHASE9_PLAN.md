# DataSipper Stream Detection - Phase 9 Plan

**Phase:** Testing & Documentation
**Effort:** 4-5 days
**Priority:** Critical (productionization)
**Status:** Not started
**Depends On:** Phases 6, 7, 8

## Overview

Phase 9 validates the entire stream detection system through comprehensive testing and creates user-facing documentation. This phase ensures reliability, performance, and usability of the feature before release.

## Goals

1. **Unit Tests**: Test all C++ and TypeScript components
2. **Integration Tests**: Test end-to-end flows
3. **Browser Tests**: Test with real web pages
4. **Performance Tests**: Measure and optimize performance
5. **Memory Tests**: Check for leaks with ASAN/LSAN
6. **User Documentation**: How-to guides and API docs
7. **Developer Documentation**: Architecture and maintenance docs

## Test Strategy

```
┌─────────────────────────────────────────────────────────────┐
│                    Testing Pyramid                           │
│                                                              │
│                        ┌─────┐                              │
│                        │ E2E │                              │
│                        └─────┘                              │
│                   Browser Tests                             │
│                   (10-15 tests)                             │
│                                                              │
│                  ┌───────────────┐                          │
│                  │  Integration  │                          │
│                  └───────────────┘                          │
│              Integration Tests                              │
│              (30-40 tests)                                  │
│                                                              │
│          ┌───────────────────────────┐                      │
│          │      Unit Tests           │                      │
│          └───────────────────────────┘                      │
│          C++ & TypeScript Unit Tests                        │
│          (100-150 tests)                                    │
└─────────────────────────────────────────────────────────────┘
```

## Unit Tests

### C++ Unit Tests

#### 1. DOMStreamTracker Tests
**Location:** `components/datasipper/streaming/dom_stream_tracker_unittest.cc`

```cpp
#include "components/datasipper/streaming/dom_stream_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "content/public/test/test_renderer_host.h"

namespace datasipper {

class DOMStreamTrackerTest : public testing::Test {
 protected:
  void SetUp() override {
    // Setup test environment
  }
};

TEST_F(DOMStreamTrackerTest, InjectScriptCreatesTracker) {
  // Test that JavaScript is injected correctly
  // Verify tracker object is created
}

TEST_F(DOMStreamTrackerTest, DetectsRegularUpdates) {
  // Simulate regular DOM mutations
  // Verify stream is detected
  // Check timing statistics
}

TEST_F(DOMStreamTrackerTest, CalculatesTimingCorrectly) {
  // Test interval calculation
  // Test standard deviation
  // Test coefficient of variation
}

TEST_F(DOMStreamTrackerTest, FiltersIrregularPatterns) {
  // Simulate irregular updates
  // Verify not detected as stream
}

TEST_F(DOMStreamTrackerTest, HandlesMultipleElements) {
  // Track multiple elements simultaneously
  // Verify separate streams detected
}

TEST_F(DOMStreamTrackerTest, MessagePollingWorks) {
  // Test GetMessages() returns pending messages
  // Verify messages cleared after retrieval
}

TEST_F(DOMStreamTrackerTest, CleanupOnDisable) {
  // Enable tracking, detect stream
  // Disable tracking
  // Verify cleanup complete
}
```

#### 2. NetworkStreamAnalyzer Tests
**Location:** `components/datasipper/streaming/network_stream_analyzer_unittest.cc`

```cpp
TEST_F(NetworkStreamAnalyzerTest, DetectsHTTPPolling) {
  // Send repeated HTTP requests to same URL
  // Verify polling pattern detected
}

TEST_F(NetworkStreamAnalyzerTest, NormalizesURLs) {
  // Test URL normalization
  // Verify query params removed
  // Verify same endpoint grouped
}

TEST_F(NetworkStreamAnalyzerTest, DetectsWebSocketConnections) {
  // Simulate WebSocket connect event
  // Verify stream detected
}

TEST_F(NetworkStreamAnalyzerTest, TracksWebSocketMessages) {
  // Send WebSocket messages
  // Verify message counting
}

TEST_F(NetworkStreamAnalyzerTest, CalculatesPollingStatistics) {
  // Send requests with known intervals
  // Verify avg_interval_ms correct
  // Verify std_deviation correct
  // Verify is_regular flag correct
}

TEST_F(NetworkStreamAnalyzerTest, FiltersNonStreams) {
  // Send one-off requests
  // Verify not detected as stream
}

TEST_F(NetworkStreamAnalyzerTest, HandlesDisconnect) {
  // Connect WebSocket
  // Send messages
  // Disconnect
  // Verify stream marked inactive
}

TEST_F(NetworkStreamAnalyzerTest, PeriodicCheckingWorks) {
  // Enable analysis
  // Wait for check interval
  // Verify CheckForNewStreams called
}
```

#### 3. StreamRegistry Tests
**Location:** `components/datasipper/streaming/stream_registry_unittest.cc`

```cpp
TEST_F(StreamRegistryTest, RegisterStream) {
  // Register new stream
  // Verify stream_id returned
  // Verify stream stored
}

TEST_F(StreamRegistryTest, DeduplicatesStreams) {
  // Register same stream twice
  // Verify only one stored
  // Verify first registration returned
}

TEST_F(StreamRegistryTest, GetStreamById) {
  // Register stream
  // Retrieve by ID
  // Verify correct stream returned
}

TEST_F(StreamRegistryTest, GetAllStreams) {
  // Register multiple streams
  // GetAllStreams()
  // Verify all returned
}

TEST_F(StreamRegistryTest, GetActiveStreams) {
  // Register active and inactive streams
  // GetActiveStreams()
  // Verify only active returned
}

TEST_F(StreamRegistryTest, GetStreamsByType) {
  // Register different stream types
  // GetStreamsByType(DOM_MUTATION)
  // Verify only DOM streams returned
}

TEST_F(StreamRegistryTest, PublishEvent) {
  // Register stream
  // PublishEvent()
  // Verify event added to recent_events
  // Verify event_count_total incremented
}

TEST_F(StreamRegistryTest, SubscribeToStream) {
  // Register stream
  // Subscribe with callback
  // Verify subscription_id returned
}

TEST_F(StreamRegistryTest, NotifySubscribers) {
  // Subscribe to stream
  // Publish event
  // Verify callback invoked
  // Verify correct event data
}

TEST_F(StreamRegistryTest, EventFiltering) {
  // Subscribe with event_type_filter
  // Publish matching and non-matching events
  // Verify only matching events delivered
}

TEST_F(StreamRegistryTest, MaxEventsAutoUnsubscribe) {
  // Subscribe with max_events = 5
  // Publish 10 events
  // Verify unsubscribed after 5
}

TEST_F(StreamRegistryTest, MaxDurationAutoUnsubscribe) {
  // Subscribe with max_duration = 1 second
  // Wait 2 seconds
  // Publish event
  // Verify not delivered (unsubscribed)
}

TEST_F(StreamRegistryTest, BackfillRecentEvents) {
  // Publish 5 events
  // Subscribe with include_recent_events = true
  // Verify 5 events delivered immediately
}

TEST_F(StreamRegistryTest, PatternMatching) {
  // Register streams with different patterns
  // MatchUrlPattern("https://example.com/*")
  // Verify correct streams matched
}

TEST_F(StreamRegistryTest, RingBufferLimit) {
  // Publish 150 events (> kMaxRecentEvents)
  // Verify only last 100 stored
}
```

#### 4. StreamDatabaseSync Tests
**Location:** `components/datasipper/streaming/stream_database_sync_unittest.cc`

```cpp
TEST_F(StreamDatabaseSyncTest, Initialize) {
  // Initialize sync
  // Verify tables created
}

TEST_F(StreamDatabaseSyncTest, SaveStream) {
  // Save stream
  // Query database directly
  // Verify stream stored correctly
}

TEST_F(StreamDatabaseSyncTest, LoadStreams) {
  // Insert streams directly to DB
  // LoadStreamsFromDatabase()
  // Verify streams in registry
}

TEST_F(StreamDatabaseSyncTest, UpdateStream) {
  // Save stream
  // Update stream
  // Verify changes persisted
}

TEST_F(StreamDatabaseSyncTest, DeleteStream) {
  // Save stream
  // DeleteStream()
  // Verify removed from database
}

TEST_F(StreamDatabaseSyncTest, SaveEvent) {
  // Save stream
  // SaveEvent()
  // Verify event in database
}

TEST_F(StreamDatabaseSyncTest, GetStreamEvents) {
  // Save stream with events
  // GetStreamEvents()
  // Verify correct events returned
  // Verify ordered by timestamp
}

TEST_F(StreamDatabaseSyncTest, DeleteOldEvents) {
  // Insert events with old timestamps
  // DeleteOldEvents(7 days)
  // Verify old events removed
}

TEST_F(StreamDatabaseSyncTest, DeleteInactiveStreams) {
  // Create inactive stream
  // Wait
  // DeleteInactiveStreams(1 hour)
  // Verify stream removed
}
```

### TypeScript Unit Tests

#### 5. StreamsTab Tests
**Location:** `chrome/browser/resources/side_panel/datasipper/streams/streams_tab_test.ts`

```typescript
import { StreamsTabElement } from './streams_tab.js';
import { assertEquals, assertFalse } from 'chrome://webui-test/chai_assert.js';

suite('StreamsTabTest', () => {
  let element: StreamsTabElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('streams-tab') as StreamsTabElement;
    document.body.appendChild(element);
  });

  test('LoadsStreamsOnConnect', async () => {
    // Mock getDetectedStreams()
    // Verify streams loaded
    // Verify loading state cleared
  });

  test('FiltersActiveStreams', () => {
    element.streams = [
      { streamId: '1', isActive: true, /* ... */ },
      { streamId: '2', isActive: false, /* ... */ },
    ];
    element.filter = { activeOnly: true, searchQuery: '' };

    const filtered = element.filteredStreams;
    assertEquals(1, filtered.length);
    assertEquals('1', filtered[0].streamId);
  });

  test('SearchFiltersStreams', () => {
    element.streams = [
      { streamId: '1', label: 'Stock Price', /* ... */ },
      { streamId: '2', label: 'Weather Data', /* ... */ },
    ];
    element.filter = { activeOnly: false, searchQuery: 'stock' };

    const filtered = element.filteredStreams;
    assertEquals(1, filtered.length);
    assertEquals('Stock Price', filtered[0].label);
  });

  test('ListensForStreamDetected', async () => {
    const callback = DataSipperUICallbackRouter.getInstance()
      .onStreamDetected.addListener;

    // Trigger onStreamDetected
    // Verify new stream added to list
  });

  test('ListensForStreamUpdated', async () => {
    element.streams = [{ streamId: '1', eventCount: 10, /* ... */ }];

    // Trigger onStreamUpdated with updated stream
    // Verify stream updated in list
  });
});
```

#### 6. StreamCard Tests
**Location:** `chrome/browser/resources/side_panel/datasipper/streams/stream_card_test.ts`

```typescript
suite('StreamCardTest', () => {
  test('DisplaysStreamInfo', () => {
    const stream = {
      streamId: '1',
      label: 'Test Stream',
      type: StreamType.DOM_MUTATION,
      isActive: true,
      /* ... */
    };

    const card = document.createElement('stream-card');
    card.stream = stream;
    document.body.appendChild(card);

    // Verify label displayed
    // Verify type icon correct
    // Verify status icon correct
  });

  test('FormatsInterval', () => {
    // Test 500ms → "500ms"
    // Test 5000ms → "5s"
    // Test 60000ms → "1min"
  });

  test('DispatchesViewEvent', () => {
    const card = document.createElement('stream-card');
    card.stream = testStream;

    let eventFired = false;
    card.addEventListener('view', () => { eventFired = true; });

    // Click view button
    const button = card.shadowRoot!.querySelector('.view-button');
    button.click();

    assertTrue(eventFired);
  });
});
```

## Integration Tests

### End-to-End Stream Detection

**Location:** `components/datasipper/datasipper_integration_test.cc`

```cpp
class DataSipperIntegrationTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    // Enable DataSipper feature flag
    feature_list_.InitAndEnableFeature(features::kDataSipperEnabled);
    InProcessBrowserTest::SetUp();
  }

  DataSipperService* GetService() {
    // Get service from profile
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DataSipperIntegrationTest, DetectsDOMStream) {
  // Load test page with updating element
  // Wait for stream detection
  // Verify stream in registry
  // Verify stream in database
}

IN_PROC_BROWSER_TEST_F(DataSipperIntegrationTest, DetectsNetworkPolling) {
  // Load page that polls API
  // Wait for detection
  // Verify network stream detected
}

IN_PROC_BROWSER_TEST_F(DataSipperIntegrationTest, DetectsWebSocket) {
  // Load page with WebSocket
  // Wait for connection
  // Verify WebSocket stream detected
}

IN_PROC_BROWSER_TEST_F(DataSipperIntegrationTest, PersistsAcrossRestart) {
  // Detect stream
  // Restart browser
  // Verify stream restored from database
}

IN_PROC_BROWSER_TEST_F(DataSipperIntegrationTest, StreamSubscription) {
  // Detect stream
  // Subscribe via Mojo
  // Publish event
  // Verify event received in UI
}
```

## Browser Tests

### Real Website Tests

**Location:** `components/datasipper/browser_test.cc`

```cpp
IN_PROC_BROWSER_TEST_F(DataSipperBrowserTest, DetectsStockTicker) {
  // Navigate to real stock ticker site
  // Wait for detection
  // Verify regular pattern detected
}

IN_PROC_BROWSER_TEST_F(DataSipperBrowserTest, DetectsChatApplication) {
  // Navigate to chat app with WebSocket
  // Verify WebSocket stream detected
  // Send message
  // Verify event captured
}

IN_PROC_BROWSER_TEST_F(DataSipperBrowserTest, DetectsSportsScores) {
  // Navigate to live sports scores
  // Verify polling pattern detected
}

IN_PROC_BROWSER_TEST_F(DataSipperBrowserTest, UIDisplaysStreams) {
  // Detect multiple streams
  // Open DataSipper side panel
  // Navigate to Streams tab
  // Verify streams visible in UI
}
```

## Performance Tests

### Benchmarks

**Location:** `components/datasipper/streaming/stream_performance_test.cc`

```cpp
TEST(StreamPerformanceTest, RegistryScalability) {
  StreamRegistry registry;
  base::ElapsedTimer timer;

  // Register 10,000 streams
  for (int i = 0; i < 10000; ++i) {
    StreamDefinition stream;
    stream.stream_id = base::StringPrintf("stream_%d", i);
    // ... fill stream ...
    registry.RegisterStream(stream);
  }

  auto elapsed = timer.Elapsed();
  EXPECT_LT(elapsed, base::Milliseconds(1000))
      << "Registering 10k streams took " << elapsed;

  // Test GetAllStreams performance
  timer = base::ElapsedTimer();
  auto streams = registry.GetAllStreams();
  elapsed = timer.Elapsed();
  EXPECT_LT(elapsed, base::Milliseconds(100))
      << "GetAllStreams took " << elapsed;
}

TEST(StreamPerformanceTest, EventPublishingThroughput) {
  StreamRegistry registry;

  // Register stream with 100 subscribers
  StreamDefinition stream;
  stream.stream_id = "test_stream";
  registry.RegisterStream(stream);

  for (int i = 0; i < 100; ++i) {
    registry.Subscribe(stream.stream_id,
                      base::BindRepeating([](const StreamEvent&) {}));
  }

  // Publish 1000 events
  base::ElapsedTimer timer;
  for (int i = 0; i < 1000; ++i) {
    base::Value data(base::Value::Type::DICT);
    registry.PublishEvent(stream.stream_id, data);
  }

  auto elapsed = timer.Elapsed();
  EXPECT_LT(elapsed, base::Milliseconds(500))
      << "Publishing 1000 events to 100 subscribers took " << elapsed;
}

TEST(StreamPerformanceTest, DatabaseSyncPerformance) {
  // Test database write performance
  // Save 1000 streams
  // Measure time
  // Verify < 2 seconds
}
```

### Memory Tests

**Run with ASAN/LSAN:**
```bash
# Build with ASAN
gn args out/DataSipper
# Add: is_asan = true

ninja -C out/DataSipper chrome

# Run tests
out/DataSipper/browser_tests --gtest_filter=DataSipper*

# Check for memory leaks
# ASAN will report any leaks automatically
```

**Memory Leak Checks:**
```cpp
TEST(StreamMemoryTest, NoLeaksOnStreamDetection) {
  // Create service
  // Detect 100 streams
  // Destroy service
  // ASAN will detect leaks
}

TEST(StreamMemoryTest, NoLeaksOnSubscription) {
  // Subscribe to stream
  // Publish 1000 events
  // Unsubscribe
  // Verify cleanup
}
```

## Test Coverage Goals

| Component | Coverage Target |
|-----------|----------------|
| DOMStreamTracker | >85% |
| NetworkStreamAnalyzer | >90% |
| StreamRegistry | >95% |
| StreamDatabaseSync | >90% |
| DataSipperService | >80% |
| PageHandler (Mojo) | >85% |
| UI Components (TS) | >75% |
| **Overall** | **>85%** |

## Documentation

### User Documentation

#### 1. Feature Overview
**Location:** `docs/datasipper/stream_detection.md`

```markdown
# Automatic Stream Detection

DataSipper automatically detects streaming data on web pages, allowing you to
monitor and capture real-time information without writing code.

## What are Streams?

Streams are data sources that update regularly, such as:
- Stock tickers
- Live sports scores
- Chat messages
- Server status dashboards
- Real-time analytics

## Types of Detected Streams

### DOM Mutation Streams
Elements on the page that change regularly. DataSipper analyzes the timing
patterns to determine if an element is a stream.

Example: A stock price that updates every 5 seconds.

### Network Polling Streams
HTTP requests made repeatedly to the same endpoint. Common in older web apps
that don't use WebSockets.

Example: An app checking `/api/updates` every 30 seconds.

### WebSocket Streams
Real-time WebSocket connections with message traffic.

Example: A chat application using WebSocket for live messages.

## How to Use Stream Detection

1. **Open DataSipper Side Panel**
   - Click the DataSipper icon in Chrome toolbar
   - Or right-click page → "DataSipper"

2. **Navigate to Streams Tab**
   - Click "Streams" in the tab bar

3. **View Detected Streams**
   - Active streams shown with 🟢 icon
   - Click a stream to see details

4. **Create Workflow from Stream**
   - Click "Create Workflow" on any stream
   - DataSipper generates workflow config automatically
   - Customize actions (save to CSV, webhook, etc.)

## Understanding Stream Statistics

- **Avg Interval**: Average time between updates
- **Std Deviation**: Variation in timing (lower = more regular)
- **Regularity**: Percentage showing how consistent the timing is
- **Event Count**: Total number of updates observed

## Tips

- Regular streams (>90% regularity) are best for workflows
- Event-driven streams work but may be unpredictable
- Use filters to find specific stream types
- Subscribe to see live data as it arrives
```

#### 2. Workflow Integration Guide
**Location:** `docs/datasipper/workflow_stream_integration.md`

```markdown
# Creating Workflows from Streams

Once DataSipper detects a stream, you can create workflows to automatically
process the data.

## Quick Start

1. Detect a stream (e.g., stock price ticker)
2. Click "Create Workflow from This Stream"
3. DataSipper auto-generates workflow config:
   - Stream source
   - Data extraction
   - Sample actions

4. Customize actions:
   - Save to CSV
   - Send to webhook
   - Trigger alerts
   - Export to Google Sheets

## Example: Stock Price Monitoring

```json
{
  "name": "Monitor AAPL Stock Price",
  "source": {
    "type": "stream",
    "stream_id": "dom_mutation_stock_price_abc123"
  },
  "actions": [
    {
      "type": "conditional",
      "condition": "data.price > 200",
      "action": {
        "type": "webhook",
        "url": "https://myserver.com/alert",
        "method": "POST"
      }
    },
    {
      "type": "csv_export",
      "filename": "stock_prices.csv",
      "append": true
    }
  ]
}
```

## Advanced: Filtering Events

Filter stream events before processing:

```json
{
  "source": {
    "type": "stream",
    "stream_id": "websocket_chat_xyz789",
    "filter": {
      "event_type": "message",
      "max_events": 100
    }
  }
}
```
```

### Developer Documentation

#### 3. Architecture Overview
**Location:** `docs/datasipper/dev/stream_detection_architecture.md`

```markdown
# Stream Detection Architecture

## Component Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Browser Process                           │
│                                                              │
│  DataSipperService                                          │
│  ├─ StreamRegistry (in-memory)                              │
│  ├─ StreamDatabaseSync (persistence)                        │
│  ├─ NetworkStreamAnalyzer (C++)                             │
│  └─ Per-tab PageDataDetector                                │
│      └─ DOMStreamTracker (JavaScript)                       │
│                                                              │
│  DataSipperPageHandler (Mojo endpoint)                      │
└─────────────────────────────────────────────────────────────┘
         │                                  │
         │ Mojo IPC                         │ JavaScript injection
         ▼                                  ▼
┌─────────────────────┐         ┌─────────────────────────┐
│   Renderer Process  │         │   Renderer Process      │
│   (WebUI)           │         │   (Web Page)            │
│                     │         │                         │
│   streams_tab.ts    │         │   DOMStreamTracker.js   │
│   stream_card.ts    │         │   (injected)            │
└─────────────────────┘         └─────────────────────────┘
```

## Data Flow

1. **Detection**
   - DOM changes → DOMStreamTracker (JS)
   - Network requests → NetworkStreamAnalyzer (C++)

2. **Analysis**
   - Timing statistics calculated
   - Pattern recognition (regular vs. event-driven)

3. **Registration**
   - Stream reported to DataSipperService
   - StreamRegistry stores in-memory
   - StreamDatabaseSync persists to DB

4. **Notification**
   - PageHandler notified via callback
   - WebUI updated via Mojo

5. **Subscription**
   - Workflows subscribe via StreamRegistry
   - Real-time events delivered via callbacks

## Key Classes

### StreamRegistry
Central registry for all detected streams. Provides:
- Stream registration and deduplication
- Subscription management
- Event publication
- Pattern matching

### NetworkStreamAnalyzer
Analyzes network traffic to detect:
- HTTP polling patterns
- WebSocket connections
- Timing statistics

### DOMStreamTracker
JavaScript injected into pages to detect:
- DOM mutations
- Element update patterns
- Timing analysis

## Threading Model

- Main Thread: DataSipperService, StreamRegistry
- IO Thread: Network event capture
- Renderer Thread: DOMStreamTracker
- DB Thread: Database operations

## Extension Points

To add a new stream type:

1. Update `StreamType` enum in stream_types.h
2. Implement detection logic
3. Report via `OnStreamDetected` callback
4. Update UI to display new type
```

#### 4. Testing Guide
**Location:** `docs/datasipper/dev/testing_guide.md`

```markdown
# DataSipper Testing Guide

## Running Tests

### Unit Tests
```bash
# C++ unit tests
ninja -C out/DataSipper components_unittests
out/DataSipper/components_unittests --gtest_filter=*DataSipper*

# TypeScript unit tests
npm test -- --test-filter=streams
```

### Browser Tests
```bash
ninja -C out/DataSipper browser_tests
out/DataSipper/browser_tests --gtest_filter=DataSipper*
```

### Memory Tests
```bash
# Build with ASAN
gn args out/DataSipper_asan
# Set: is_asan = true, is_debug = false

ninja -C out/DataSipper_asan browser_tests
out/DataSipper_asan/browser_tests --gtest_filter=DataSipper*
```

## Writing New Tests

### Adding C++ Unit Test
1. Create *_unittest.cc file
2. Include appropriate headers
3. Use TEST() or TEST_F() macros
4. Add to BUILD.gn sources

### Adding Browser Test
1. Create *_browsertest.cc file
2. Extend InProcessBrowserTest
3. Use IN_PROC_BROWSER_TEST_F macro
4. Add to BUILD.gn test sources

### Mock Objects

Use these mocks for testing:
- MockStreamRegistry
- MockDataSipperService
- FakeNetworkEvent

## Performance Benchmarking

```bash
# Run performance tests
out/DataSipper/components_perftests --gtest_filter=Stream*

# Profile with perf
perf record out/DataSipper/chrome --enable-features=DataSipperEnabled
perf report
```
```

## Success Criteria

Phase 9 complete when:
- ✅ >85% overall test coverage
- ✅ All unit tests passing
- ✅ All integration tests passing
- ✅ All browser tests passing
- ✅ No memory leaks (ASAN clean)
- ✅ Performance benchmarks met:
  - Registry: <1ms for 1000 streams
  - Event publish: <0.5ms per event
  - Database save: <10ms per stream
- ✅ User documentation complete
- ✅ Developer documentation complete
- ✅ API documentation generated

## Files to Create

**Test Files:**
- `dom_stream_tracker_unittest.cc` (~400 lines)
- `network_stream_analyzer_unittest.cc` (~500 lines)
- `stream_registry_unittest.cc` (~800 lines)
- `stream_database_sync_unittest.cc` (~400 lines)
- `datasipper_integration_test.cc` (~600 lines)
- `datasipper_browser_test.cc` (~400 lines)
- `stream_performance_test.cc` (~300 lines)
- `streams_tab_test.ts` (~300 lines)
- `stream_card_test.ts` (~200 lines)
- `stream_details_test.ts` (~200 lines)

**Documentation:**
- `docs/datasipper/stream_detection.md` (~500 lines)
- `docs/datasipper/workflow_stream_integration.md` (~400 lines)
- `docs/datasipper/dev/architecture.md` (~600 lines)
- `docs/datasipper/dev/testing_guide.md` (~300 lines)
- `docs/datasipper/api/stream_registry.md` (~400 lines)

**Total:** ~5,900 lines

## Timeline

| Day | Tasks |
|-----|-------|
| Day 1 | C++ unit tests (DOM, Network) |
| Day 2 | C++ unit tests (Registry, DB) |
| Day 3 | Integration & browser tests |
| Day 4 | Performance tests, memory checks |
| Day 5 | Documentation, polish |

## Quality Gates

Before marking Phase 9 complete:

1. **Code Coverage**: Run coverage report, verify >85%
2. **Memory**: ASAN clean on all tests
3. **Performance**: All benchmarks under target
4. **Documentation**: All docs reviewed and complete
5. **Manual Testing**: Test on 5+ real websites
6. **Accessibility**: Run axe-core, verify WCAG 2.1 AA
7. **Security**: Run static analysis, verify no warnings

## Post-Release Monitoring

After Phase 9, set up:
- Crash reporting for stream detection
- Performance metrics in UMA
- User feedback collection
- Bug triage process

## Notes

- Write tests as you implement (TDD)
- Target 1 test per public method minimum
- Integration tests cover happy path + edge cases
- Browser tests use real websites when possible
- Document all test data sources
- Keep tests fast (<100ms per unit test)
- Use mocks to isolate components
