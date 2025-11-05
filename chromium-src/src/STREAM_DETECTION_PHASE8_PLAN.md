# DataSipper Stream Detection - Phase 8 Plan

**Phase:** UI Components
**Effort:** 5-6 days
**Priority:** High (user-facing functionality)
**Status:** Not started
**Depends On:** Phase 7 (Database & Mojo)

## Overview

Phase 8 implements the WebUI components that allow users to view detected streams, monitor real-time events, and create workflows from streams. This phase brings stream detection to life with a polished, interactive user interface.

## Goals

1. **Streams Tab**: New tab in side panel showing all detected streams
2. **Stream List**: Display active and inactive streams with key metrics
3. **Stream Details**: Detailed view with timing, events, and statistics
4. **Real-Time Updates**: Live event display as data arrives
5. **Workflow Creation**: Export stream as workflow configuration
6. **Stream Control**: Pause/resume, delete streams
7. **Event History**: Browse historical events with filtering

## UI Architecture

```
┌─────────────────────────────────────────────────────────────┐
│              DataSipper Side Panel                           │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ Tab Navigation                                       │   │
│  │ [Extract] [Workflows] [Streams] [Network] [Settings]│   │
│  └─────────────────────────────────────────────────────┘   │
│                         │                                    │
│                         ▼ (Streams tab active)               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              Streams Tab Component                   │   │
│  │                                                      │   │
│  │  ┌────────────────────────────────────────────┐    │   │
│  │  │ Stream Filter Bar                          │    │   │
│  │  │ [All] [DOM] [Network] [WebSocket]          │    │   │
│  │  │ [🔍 Search...] [Active Only ☑]             │    │   │
│  │  └────────────────────────────────────────────┘    │   │
│  │                                                      │   │
│  │  ┌────────────────────────────────────────────┐    │   │
│  │  │ Stream List                                │    │   │
│  │  │                                            │    │   │
│  │  │ ┌────────────────────────────────────┐   │    │   │
│  │  │ │ 🟢 Stock Price Ticker              │   │    │   │
│  │  │ │ DOM Mutation • every 5s            │   │    │   │
│  │  │ │ https://stocks.com                 │   │    │   │
│  │  │ │ 142 events • Regular pattern       │   │    │   │
│  │  │ │ [View] [Create Workflow] [⋯]       │   │    │   │
│  │  │ └────────────────────────────────────┘   │    │   │
│  │  │                                            │    │   │
│  │  │ ┌────────────────────────────────────┐   │    │   │
│  │  │ │ 🟢 Live Chat Messages              │   │    │   │
│  │  │ │ WebSocket • Real-time              │   │    │   │
│  │  │ │ ws://chat.app.com                  │   │    │   │
│  │  │ │ 89 events • Event-driven           │   │    │   │
│  │  │ │ [View] [Create Workflow] [⋯]       │   │    │   │
│  │  │ └────────────────────────────────────┘   │    │   │
│  │  │                                            │    │   │
│  │  │ ⚪ API Polling (inactive)              │    │   │
│  │  │ Network Pattern • every 30s            │    │   │
│  │  │ ...                                    │    │   │
│  │  └────────────────────────────────────────────┘    │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │         Stream Details Panel (when clicked)          │   │
│  │                                                      │   │
│  │  Stream: Stock Price Ticker                         │   │
│  │  Type: DOM Mutation                                 │   │
│  │  Pattern: #stock-price                              │   │
│  │  URL: https://stocks.com/AAPL                       │   │
│  │                                                      │   │
│  │  ┌────────────────────────────────────────────┐    │   │
│  │  │ Timing Statistics                          │    │   │
│  │  │                                            │    │   │
│  │  │ Avg Interval:    5,000 ms                  │    │   │
│  │  │ Std Deviation:   200 ms                    │    │   │
│  │  │ Regularity:      96% (Regular)             │    │   │
│  │  │ Total Events:    142                       │    │   │
│  │  │ First Seen:      2 hours ago               │    │   │
│  │  │ Last Seen:       2 seconds ago             │    │   │
│  │  └────────────────────────────────────────────┘    │   │
│  │                                                      │   │
│  │  ┌────────────────────────────────────────────┐    │   │
│  │  │ Recent Events (live)                       │    │   │
│  │  │                                            │    │   │
│  │  │ 14:32:15  {"price": 185.23, "change": +0.5}│    │   │
│  │  │ 14:32:10  {"price": 185.18, "change": +0.3}│    │   │
│  │  │ 14:32:05  {"price": 185.15, "change": +0.2}│    │   │
│  │  │ ...                                        │    │   │
│  │  │                                            │    │   │
│  │  │ [Load More] [Export Events] [Clear]        │    │   │
│  │  └────────────────────────────────────────────┘    │   │
│  │                                                      │   │
│  │  [Create Workflow from This Stream]                 │   │
│  │  [Pause Stream] [Delete Stream]                     │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Component Structure

### File Organization

```
chrome/browser/resources/side_panel/datasipper/
├── datasipper_app.ts                 (main app, existing)
├── datasipper_ui.ts                  (UI coordinator, existing)
├── streams/                          (NEW)
│   ├── streams_tab.ts                (main streams tab component)
│   ├── stream_list.ts                (list of streams)
│   ├── stream_card.ts                (individual stream card)
│   ├── stream_details.ts             (detailed stream view)
│   ├── stream_event_viewer.ts        (event display component)
│   ├── stream_stats_panel.ts         (statistics display)
│   ├── stream_filter.ts              (filter/search component)
│   ├── create_workflow_dialog.ts     (workflow creation)
│   └── streams.css                   (stream-specific styles)
└── types/                            (NEW)
    └── stream_types.ts               (TypeScript type definitions)
```

### Type Definitions

**Location:** `chrome/browser/resources/side_panel/datasipper/types/stream_types.ts`

```typescript
// TypeScript types matching Mojo definitions

export enum StreamType {
  DOM_MUTATION = 0,
  NETWORK_PATTERN = 1,
  WEBSOCKET_STREAM = 2,
}

export enum DataType {
  UNKNOWN = 0,
  JSON = 1,
  XML = 2,
  CSV = 3,
  TEXT = 4,
  BINARY = 5,
}

export interface StreamTimingInfo {
  avgIntervalMs: number;
  stdDeviation: number;
  isRegular: boolean;
}

export interface StreamInfo {
  streamId: string;
  label: string;
  type: StreamType;
  sourcePattern: string;
  url: string;
  firstSeen: number;  // Unix timestamp ms
  lastSeen: number;
  timing: StreamTimingInfo;
  isActive: boolean;
  eventCount: number;
  dataType: DataType;
}

export interface StreamEventInfo {
  streamId: string;
  timestamp: number;
  eventType: string;
  data: string;  // JSON string
}

export interface StreamSubscription {
  subscriptionId: string;
  streamId: string;
}

// UI-specific types
export interface StreamFilter {
  type?: StreamType;
  activeOnly: boolean;
  searchQuery: string;
}

export interface StreamStats {
  totalStreams: number;
  activeStreams: number;
  totalEvents: number;
  avgInterval: number;
}
```

## Component Implementation

### 1. StreamsTab Component

**Location:** `chrome/browser/resources/side_panel/datasipper/streams/streams_tab.ts`

```typescript
import { html, css, LitElement } from 'chrome://resources/lit/v3_0/lit.rollup.js';
import { StreamInfo, StreamFilter, StreamType } from '../types/stream_types.js';

export class StreamsTabElement extends LitElement {
  static properties = {
    streams: { type: Array },
    selectedStream: { type: Object },
    filter: { type: Object },
    loading: { type: Boolean },
  };

  streams: StreamInfo[] = [];
  selectedStream: StreamInfo | null = null;
  filter: StreamFilter = {
    activeOnly: true,
    searchQuery: '',
  };
  loading: boolean = true;

  private pageHandler = DataSipperUIHandler.getRemote();

  connectedCallback() {
    super.connectedCallback();
    this.loadStreams();
    this.setupEventListeners();
  }

  async loadStreams() {
    this.loading = true;
    const { streams } = await this.pageHandler.getDetectedStreams();
    this.streams = streams;
    this.loading = false;
  }

  setupEventListeners() {
    // Listen for new streams
    DataSipperUICallbackRouter.getInstance().onStreamDetected.addListener(
      (stream: StreamInfo) => {
        this.streams = [...this.streams, stream];
      }
    );

    // Listen for stream updates
    DataSipperUICallbackRouter.getInstance().onStreamUpdated.addListener(
      (stream: StreamInfo) => {
        const index = this.streams.findIndex(s => s.streamId === stream.streamId);
        if (index !== -1) {
          this.streams[index] = stream;
          this.streams = [...this.streams];
        }
      }
    );

    // Listen for stream deactivation
    DataSipperUICallbackRouter.getInstance().onStreamDeactivated.addListener(
      (streamId: string) => {
        const stream = this.streams.find(s => s.streamId === streamId);
        if (stream) {
          stream.isActive = false;
          this.streams = [...this.streams];
        }
      }
    );
  }

  get filteredStreams(): StreamInfo[] {
    return this.streams.filter(stream => {
      // Filter by active status
      if (this.filter.activeOnly && !stream.isActive) {
        return false;
      }

      // Filter by type
      if (this.filter.type !== undefined && stream.type !== this.filter.type) {
        return false;
      }

      // Filter by search query
      if (this.filter.searchQuery) {
        const query = this.filter.searchQuery.toLowerCase();
        return (
          stream.label.toLowerCase().includes(query) ||
          stream.url.toLowerCase().includes(query) ||
          stream.sourcePattern.toLowerCase().includes(query)
        );
      }

      return true;
    });
  }

  onStreamSelected(stream: StreamInfo) {
    this.selectedStream = stream;
  }

  onFilterChanged(newFilter: Partial<StreamFilter>) {
    this.filter = { ...this.filter, ...newFilter };
  }

  render() {
    return html`
      <div class="streams-tab">
        <stream-filter
          .filter=${this.filter}
          @filter-changed=${(e: CustomEvent) => this.onFilterChanged(e.detail)}
        ></stream-filter>

        ${this.loading ? html`
          <div class="loading">
            <div class="spinner"></div>
            <p>Loading streams...</p>
          </div>
        ` : html`
          <stream-list
            .streams=${this.filteredStreams}
            .selectedStream=${this.selectedStream}
            @stream-selected=${(e: CustomEvent) => this.onStreamSelected(e.detail)}
          ></stream-list>

          ${this.selectedStream ? html`
            <stream-details
              .stream=${this.selectedStream}
              @close=${() => this.selectedStream = null}
            ></stream-details>
          ` : ''}
        `}
      </div>
    `;
  }

  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      height: 100%;
      overflow: hidden;
    }

    .streams-tab {
      display: flex;
      flex-direction: column;
      height: 100%;
      gap: 16px;
      padding: 16px;
    }

    .loading {
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      height: 100%;
      gap: 16px;
    }

    .spinner {
      width: 32px;
      height: 32px;
      border: 3px solid #f3f3f3;
      border-top: 3px solid #1a73e8;
      border-radius: 50%;
      animation: spin 1s linear infinite;
    }

    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
  `;
}

customElements.define('streams-tab', StreamsTabElement);
```

### 2. StreamCard Component

**Location:** `chrome/browser/resources/side_panel/datasipper/streams/stream_card.ts`

```typescript
export class StreamCardElement extends LitElement {
  static properties = {
    stream: { type: Object },
    selected: { type: Boolean },
  };

  stream!: StreamInfo;
  selected: boolean = false;

  private getStreamTypeIcon(): string {
    switch (this.stream.type) {
      case StreamType.DOM_MUTATION:
        return '📄';
      case StreamType.NETWORK_PATTERN:
        return '🌐';
      case StreamType.WEBSOCKET_STREAM:
        return '⚡';
      default:
        return '📊';
    }
  }

  private getStreamTypeName(): string {
    switch (this.stream.type) {
      case StreamType.DOM_MUTATION:
        return 'DOM Mutation';
      case StreamType.NETWORK_PATTERN:
        return 'Network Pattern';
      case StreamType.WEBSOCKET_STREAM:
        return 'WebSocket';
      default:
        return 'Unknown';
    }
  }

  private formatInterval(): string {
    const ms = this.stream.timing.avgIntervalMs;
    if (ms < 1000) {
      return `${Math.round(ms)}ms`;
    } else if (ms < 60000) {
      return `${Math.round(ms / 1000)}s`;
    } else {
      return `${Math.round(ms / 60000)}min`;
    }
  }

  private formatTimestamp(timestamp: number): string {
    const now = Date.now();
    const diff = now - timestamp;
    const seconds = Math.floor(diff / 1000);

    if (seconds < 60) return `${seconds}s ago`;
    if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
    if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ago`;
    return `${Math.floor(seconds / 86400)}d ago`;
  }

  onCreateWorkflow() {
    this.dispatchEvent(new CustomEvent('create-workflow', {
      detail: this.stream,
      bubbles: true,
      composed: true,
    }));
  }

  render() {
    const statusIcon = this.stream.isActive ? '🟢' : '⚪';
    const patternType = this.stream.timing.isRegular ? 'Regular' : 'Event-driven';

    return html`
      <div class="stream-card ${this.selected ? 'selected' : ''}">
        <div class="stream-header">
          <span class="status-icon">${statusIcon}</span>
          <span class="type-icon">${this.getStreamTypeIcon()}</span>
          <h3 class="stream-label">${this.stream.label}</h3>
        </div>

        <div class="stream-meta">
          <span class="stream-type">${this.getStreamTypeName()}</span>
          <span class="separator">•</span>
          <span class="stream-interval">every ${this.formatInterval()}</span>
        </div>

        <div class="stream-url">${this.stream.url}</div>

        <div class="stream-stats">
          <span class="event-count">${this.stream.eventCount} events</span>
          <span class="separator">•</span>
          <span class="pattern-type">${patternType}</span>
          <span class="separator">•</span>
          <span class="last-seen">${this.formatTimestamp(this.stream.lastSeen)}</span>
        </div>

        <div class="stream-actions">
          <button class="view-button" @click=${() => this.dispatchEvent(new CustomEvent('view'))}>
            View
          </button>
          <button class="create-workflow-button" @click=${this.onCreateWorkflow}>
            Create Workflow
          </button>
          <button class="menu-button">⋯</button>
        </div>
      </div>
    `;
  }

  static styles = css`
    .stream-card {
      background: white;
      border: 1px solid #dadce0;
      border-radius: 8px;
      padding: 16px;
      cursor: pointer;
      transition: all 0.2s ease;
    }

    .stream-card:hover {
      box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
      border-color: #1a73e8;
    }

    .stream-card.selected {
      border-color: #1a73e8;
      background: #f8fbff;
    }

    .stream-header {
      display: flex;
      align-items: center;
      gap: 8px;
      margin-bottom: 8px;
    }

    .stream-label {
      font-size: 16px;
      font-weight: 500;
      margin: 0;
      flex: 1;
    }

    .stream-meta {
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 13px;
      color: #5f6368;
      margin-bottom: 4px;
    }

    .stream-url {
      font-size: 12px;
      color: #1a73e8;
      margin-bottom: 8px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .stream-stats {
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 12px;
      color: #5f6368;
      margin-bottom: 12px;
    }

    .stream-actions {
      display: flex;
      gap: 8px;
    }

    button {
      padding: 6px 12px;
      border: 1px solid #dadce0;
      border-radius: 4px;
      background: white;
      cursor: pointer;
      font-size: 13px;
      transition: all 0.2s ease;
    }

    button:hover {
      background: #f8f9fa;
    }

    .create-workflow-button {
      background: #1a73e8;
      color: white;
      border-color: #1a73e8;
    }

    .create-workflow-button:hover {
      background: #1765cc;
    }

    .separator {
      color: #dadce0;
    }
  `;
}

customElements.define('stream-card', StreamCardElement);
```

### 3. StreamDetails Component

**Location:** `chrome/browser/resources/side_panel/datasipper/streams/stream_details.ts`

```typescript
export class StreamDetailsElement extends LitElement {
  static properties = {
    stream: { type: Object },
    events: { type: Array },
    subscription: { type: Object },
  };

  stream!: StreamInfo;
  events: StreamEventInfo[] = [];
  subscription: StreamSubscription | null = null;

  private pageHandler = DataSipperUIHandler.getRemote();

  async connectedCallback() {
    super.connectedCallback();
    await this.loadEvents();
    await this.subscribeToStream();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.unsubscribeFromStream();
  }

  async loadEvents() {
    const { events } = await this.pageHandler.getStreamEvents(
      this.stream.streamId,
      50  // Load last 50 events
    );
    this.events = events;
  }

  async subscribeToStream() {
    const { subscription } = await this.pageHandler.subscribeToStream(
      this.stream.streamId
    );

    if (subscription) {
      this.subscription = subscription;

      // Listen for real-time events
      DataSipperUICallbackRouter.getInstance().onStreamEvent.addListener(
        (event: StreamEventInfo) => {
          if (event.streamId === this.stream.streamId) {
            this.events = [event, ...this.events].slice(0, 100);
          }
        }
      );
    }
  }

  async unsubscribeFromStream() {
    if (this.subscription) {
      await this.pageHandler.unsubscribeFromStream(
        this.subscription.subscriptionId
      );
      this.subscription = null;
    }
  }

  formatEventData(dataStr: string): string {
    try {
      const data = JSON.parse(dataStr);
      return JSON.stringify(data, null, 2);
    } catch {
      return dataStr;
    }
  }

  render() {
    const regularity = this.stream.timing.isRegular
      ? Math.round((1 - this.stream.timing.stdDeviation / this.stream.timing.avgIntervalMs) * 100)
      : 0;

    return html`
      <div class="stream-details">
        <div class="details-header">
          <h2>${this.stream.label}</h2>
          <button class="close-button" @click=${() => this.dispatchEvent(new CustomEvent('close'))}>
            ✕
          </button>
        </div>

        <div class="details-section">
          <h3>Stream Information</h3>
          <dl>
            <dt>Type:</dt>
            <dd>${this.getStreamTypeName()}</dd>

            <dt>Pattern:</dt>
            <dd><code>${this.stream.sourcePattern}</code></dd>

            <dt>URL:</dt>
            <dd><a href="${this.stream.url}" target="_blank">${this.stream.url}</a></dd>
          </dl>
        </div>

        <div class="details-section">
          <h3>Timing Statistics</h3>
          <div class="stats-grid">
            <div class="stat-card">
              <div class="stat-label">Avg Interval</div>
              <div class="stat-value">${this.formatInterval(this.stream.timing.avgIntervalMs)}</div>
            </div>
            <div class="stat-card">
              <div class="stat-label">Std Deviation</div>
              <div class="stat-value">${Math.round(this.stream.timing.stdDeviation)}ms</div>
            </div>
            <div class="stat-card">
              <div class="stat-label">Regularity</div>
              <div class="stat-value">${regularity}%</div>
            </div>
            <div class="stat-card">
              <div class="stat-label">Total Events</div>
              <div class="stat-value">${this.stream.eventCount}</div>
            </div>
          </div>
        </div>

        <div class="details-section">
          <h3>Recent Events ${this.subscription ? '(live)' : ''}</h3>
          <stream-event-viewer
            .events=${this.events}
            .streamType=${this.stream.type}
          ></stream-event-viewer>
        </div>

        <div class="details-actions">
          <button class="primary-button" @click=${this.onCreateWorkflow}>
            Create Workflow from This Stream
          </button>
          <button @click=${this.onPauseStream}>
            ${this.stream.isActive ? 'Pause' : 'Resume'} Stream
          </button>
          <button class="danger-button" @click=${this.onDeleteStream}>
            Delete Stream
          </button>
        </div>
      </div>
    `;
  }

  // ... styles ...
}

customElements.define('stream-details', StreamDetailsElement);
```

### 4. StreamEventViewer Component

**Location:** `chrome/browser/resources/side_panel/datasipper/streams/stream_event_viewer.ts`

```typescript
export class StreamEventViewerElement extends LitElement {
  static properties = {
    events: { type: Array },
    streamType: { type: Number },
  };

  events: StreamEventInfo[] = [];
  streamType!: StreamType;

  private formatTimestamp(timestamp: number): string {
    const date = new Date(timestamp);
    return date.toLocaleTimeString('en-US', { hour12: false });
  }

  render() {
    return html`
      <div class="event-viewer">
        ${this.events.length === 0 ? html`
          <div class="empty-state">
            <p>No events yet. Waiting for stream data...</p>
          </div>
        ` : html`
          <div class="event-list">
            ${this.events.map(event => html`
              <div class="event-item">
                <div class="event-time">${this.formatTimestamp(event.timestamp)}</div>
                <div class="event-type">${event.eventType}</div>
                <div class="event-data">
                  <pre>${this.formatEventData(event.data)}</pre>
                </div>
              </div>
            `)}
          </div>
        `}
      </div>
    `;
  }

  static styles = css`
    .event-viewer {
      max-height: 400px;
      overflow-y: auto;
      border: 1px solid #dadce0;
      border-radius: 4px;
    }

    .event-list {
      display: flex;
      flex-direction: column;
      gap: 8px;
      padding: 8px;
    }

    .event-item {
      background: #f8f9fa;
      border-radius: 4px;
      padding: 8px;
      font-size: 12px;
      font-family: 'Roboto Mono', monospace;
    }

    .event-time {
      color: #5f6368;
      margin-bottom: 4px;
    }

    .event-type {
      font-weight: 500;
      margin-bottom: 4px;
    }

    .event-data pre {
      margin: 0;
      white-space: pre-wrap;
      word-wrap: break-word;
    }

    .empty-state {
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 48px;
      color: #5f6368;
    }
  `;
}

customElements.define('stream-event-viewer', StreamEventViewerElement);
```

## Integration with DataSipper App

### Update datasipper_app.ts

Add streams tab to navigation:

```typescript
const tabs = [
  { id: 'extract', label: 'Extract', icon: '📊' },
  { id: 'workflows', label: 'Workflows', icon: '⚙️' },
  { id: 'streams', label: 'Streams', icon: '📡' },  // NEW
  { id: 'network', label: 'Network', icon: '🌐' },
  { id: 'settings', label: 'Settings', icon: '⚙️' },
];

// In render():
${this.activeTab === 'streams' ? html`
  <streams-tab></streams-tab>
` : ''}
```

## BUILD.gn Updates

Add TypeScript files to `chrome/browser/resources/side_panel/datasipper/BUILD.gn`:

```python
ts_library("build_ts") {
  sources = [
    # ... existing files ...

    # Stream components
    "streams/streams_tab.ts",
    "streams/stream_list.ts",
    "streams/stream_card.ts",
    "streams/stream_details.ts",
    "streams/stream_event_viewer.ts",
    "streams/stream_stats_panel.ts",
    "streams/stream_filter.ts",
    "streams/create_workflow_dialog.ts",
    "types/stream_types.ts",
  ]

  # ... rest of config ...
}
```

## Success Criteria

Phase 8 complete when:
- ✅ Streams tab visible in side panel
- ✅ Stream list displays all detected streams
- ✅ Stream cards show key information
- ✅ Click stream opens detailed view
- ✅ Real-time events display live
- ✅ Create workflow dialog functional
- ✅ Pause/resume/delete actions work
- ✅ Filters and search functional
- ✅ Responsive UI design
- ✅ Accessibility (keyboard nav, screen readers)
- ✅ Works in light/dark mode

## Files to Create

**New Files:**
- `streams/streams_tab.ts` (~300 lines)
- `streams/stream_list.ts` (~150 lines)
- `streams/stream_card.ts` (~250 lines)
- `streams/stream_details.ts` (~350 lines)
- `streams/stream_event_viewer.ts` (~200 lines)
- `streams/stream_stats_panel.ts` (~150 lines)
- `streams/stream_filter.ts` (~100 lines)
- `streams/create_workflow_dialog.ts` (~200 lines)
- `streams/streams.css` (~300 lines)
- `types/stream_types.ts` (~100 lines)

**Modified Files:**
- `datasipper_app.ts` (+30 lines)
- `BUILD.gn` (+10 lines)

**Total:** ~2,140 lines

## UI/UX Specifications

### Colors
- Active stream: `#34a853` (green)
- Inactive stream: `#dadce0` (gray)
- Primary action: `#1a73e8` (blue)
- Danger action: `#d93025` (red)
- Background: `#ffffff` (light), `#202124` (dark)

### Typography
- Headers: Roboto 500, 16-20px
- Body: Roboto 400, 13-14px
- Code: Roboto Mono 400, 12px

### Spacing
- Card padding: 16px
- Gap between cards: 12px
- Section margins: 24px
- Button padding: 8px 16px

## Accessibility

- ARIA labels on all interactive elements
- Keyboard navigation (Tab, Enter, Esc)
- Focus indicators
- Screen reader announcements for events
- High contrast mode support

## Performance

- Virtual scrolling for large stream lists (>100 items)
- Event viewer max 100 events (ring buffer)
- Debounced search (300ms)
- Lazy load event details
- Unsubscribe when details panel closed

## Notes

- UI built with Lit (web components)
- Follows Chrome WebUI design patterns
- Responsive down to 320px width
- Dark mode support via CSS custom properties
- Real-time updates via Mojo callbacks
