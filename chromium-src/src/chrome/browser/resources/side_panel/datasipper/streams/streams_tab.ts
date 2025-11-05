// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, css, LitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {StreamInfo, StreamType, formatTimestamp, getStreamTypeName, getStreamTypeIcon, formatInterval} from '../types/stream_types.js';

/**
 * Streams Tab - Main component for displaying detected data streams
 */
export class StreamsTabElement extends LitElement {
  static properties = {
    streams: {type: Array},
    selectedStream: {type: Object},
    loading: {type: Boolean},
    activeOnly: {type: Boolean},
  };

  streams: StreamInfo[] = [];
  selectedStream: StreamInfo|null = null;
  loading: boolean = true;
  activeOnly: boolean = true;

  private pageHandler: any;

  constructor() {
    super();
    // TODO: Get pageHandler from DataSipperUIHandler
    // this.pageHandler = DataSipperUIHandler.getRemote();
  }

  connectedCallback() {
    super.connectedCallback();
    this.loadStreams();
    this.setupEventListeners();
  }

  async loadStreams() {
    this.loading = true;
    // TODO: Uncomment when Mojo binding is available
    // const {streams} = await this.pageHandler.getDetectedStreams();
    // this.streams = streams;

    // Mock data for development
    this.streams = this.getMockStreams();
    this.loading = false;
  }

  setupEventListeners() {
    // TODO: Set up Mojo observers when available
    // DataSipperUICallbackRouter.getInstance().onStreamDetected.addListener(
    //   (stream: StreamInfo) => {
    //     this.streams = [...this.streams, stream];
    //   }
    // );
  }

  get filteredStreams(): StreamInfo[] {
    return this.streams.filter(stream => {
      if (this.activeOnly && !stream.isActive) {
        return false;
      }
      return true;
    });
  }

  onStreamSelected(stream: StreamInfo) {
    this.selectedStream = this.selectedStream?.streamId === stream.streamId ?
        null : stream;
  }

  onToggleActiveFilter(e: Event) {
    this.activeOnly = (e.target as HTMLInputElement).checked;
  }

  async onCreateWorkflow(stream: StreamInfo) {
    // TODO: Implement workflow creation
    // const {workflow} = await this.pageHandler.exportStreamAsWorkflow(stream.streamId);
    console.log('Create workflow from stream:', stream.streamId);
  }

  async onDeleteStream(stream: StreamInfo) {
    if (confirm(`Delete stream "${stream.label}"?`)) {
      // TODO: Uncomment when Mojo binding is available
      // const {success} = await this.pageHandler.deleteStream(stream.streamId);
      // if (success) {
        this.streams = this.streams.filter(s => s.streamId !== stream.streamId);
        if (this.selectedStream?.streamId === stream.streamId) {
          this.selectedStream = null;
        }
      // }
    }
  }

  render() {
    return html`
      <div class="streams-tab">
        <div class="header">
          <h2>Detected Streams</h2>
          <label class="filter-checkbox">
            <input type="checkbox"
                   ?checked=${this.activeOnly}
                   @change=${this.onToggleActiveFilter}>
            Active only
          </label>
        </div>

        ${this.loading ? html`
          <div class="loading">
            <div class="spinner"></div>
            <p>Loading streams...</p>
          </div>
        ` : this.filteredStreams.length === 0 ? html`
          <div class="empty-state">
            <p>No streams detected yet.</p>
            <p class="hint">Browse websites with dynamic content to detect streams.</p>
          </div>
        ` : html`
          <div class="stream-list">
            ${this.filteredStreams.map(stream => this.renderStreamCard(stream))}
          </div>
        `}
      </div>
    `;
  }

  renderStreamCard(stream: StreamInfo) {
    const isSelected = this.selectedStream?.streamId === stream.streamId;
    const statusIcon = stream.isActive ? '=â' : 'ª';
    const typeIcon = getStreamTypeIcon(stream.type);
    const typeName = getStreamTypeName(stream.type);
    const patternType = stream.timing.isRegular ? 'Regular' : 'Event-driven';

    return html`
      <div class="stream-card ${isSelected ? 'selected' : ''}"
           @click=${() => this.onStreamSelected(stream)}>
        <div class="stream-header">
          <span class="status-icon">${statusIcon}</span>
          <span class="type-icon">${typeIcon}</span>
          <h3 class="stream-label">${stream.label}</h3>
        </div>

        <div class="stream-meta">
          <span class="stream-type">${typeName}</span>
          <span class="separator">"</span>
          <span class="stream-interval">every ${formatInterval(stream.timing.avgIntervalMs)}</span>
        </div>

        <div class="stream-url">${stream.url}</div>

        <div class="stream-stats">
          <span>${stream.eventCount} events</span>
          <span class="separator">"</span>
          <span>${patternType}</span>
          <span class="separator">"</span>
          <span>${formatTimestamp(stream.lastSeen)}</span>
        </div>

        ${isSelected ? html`
          <div class="stream-details">
            <div class="details-section">
              <h4>Stream Information</h4>
              <dl>
                <dt>Pattern:</dt>
                <dd><code>${stream.sourcePattern}</code></dd>
                <dt>Average Interval:</dt>
                <dd>${stream.timing.avgIntervalMs.toFixed(0)}ms</dd>
                <dt>Standard Deviation:</dt>
                <dd>${stream.timing.stdDeviation.toFixed(0)}ms</dd>
                <dt>Regular:</dt>
                <dd>${stream.timing.isRegular ? 'Yes' : 'No'}</dd>
              </dl>
            </div>

            <div class="stream-actions">
              <button class="primary-button"
                      @click=${(e: Event) => {
                        e.stopPropagation();
                        this.onCreateWorkflow(stream);
                      }}>
                Create Workflow
              </button>
              <button class="danger-button"
                      @click=${(e: Event) => {
                        e.stopPropagation();
                        this.onDeleteStream(stream);
                      }}>
                Delete Stream
              </button>
            </div>
          </div>
        ` : ''}
      </div>
    `;
  }

  // Mock data for development/testing
  getMockStreams(): StreamInfo[] {
    return [
      {
        streamId: 'stream_1',
        label: 'Stock Price Ticker',
        type: StreamType.DOM_MUTATION,
        sourcePattern: '#stock-price',
        url: 'https://stocks.example.com/AAPL',
        firstSeen: BigInt(Date.now() - 7200000),
        lastSeen: BigInt(Date.now() - 2000),
        timing: {
          avgIntervalMs: 5000,
          stdDeviation: 200,
          isRegular: true,
        },
        isActive: true,
        eventCount: 142,
        dataType: 1, // JSON
      },
      {
        streamId: 'stream_2',
        label: 'Live Chat Messages',
        type: StreamType.WEBSOCKET_STREAM,
        sourcePattern: 'ws://chat.app.com',
        url: 'https://chat.app.com/room/123',
        firstSeen: BigInt(Date.now() - 3600000),
        lastSeen: BigInt(Date.now() - 5000),
        timing: {
          avgIntervalMs: 2000,
          stdDeviation: 1500,
          isRegular: false,
        },
        isActive: true,
        eventCount: 89,
        dataType: 1, // JSON
      },
      {
        streamId: 'stream_3',
        label: 'API Polling',
        type: StreamType.NETWORK_PATTERN,
        sourcePattern: '/api/status',
        url: 'https://api.example.com/status',
        firstSeen: BigInt(Date.now() - 1800000),
        lastSeen: BigInt(Date.now() - 60000),
        timing: {
          avgIntervalMs: 30000,
          stdDeviation: 500,
          isRegular: true,
        },
        isActive: false,
        eventCount: 24,
        dataType: 1, // JSON
      },
    ];
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
      padding: 16px;
      overflow-y: auto;
    }

    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 16px;
    }

    h2 {
      margin: 0;
      font-size: 20px;
      font-weight: 500;
    }

    .filter-checkbox {
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 14px;
      cursor: pointer;
    }

    .loading, .empty-state {
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      flex: 1;
      gap: 16px;
      color: #5f6368;
    }

    .hint {
      font-size: 12px;
      opacity: 0.8;
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

    .stream-list {
      display: flex;
      flex-direction: column;
      gap: 12px;
    }

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
    }

    .separator {
      color: #dadce0;
    }

    .stream-details {
      margin-top: 16px;
      padding-top: 16px;
      border-top: 1px solid #dadce0;
    }

    .details-section {
      margin-bottom: 16px;
    }

    .details-section h4 {
      font-size: 14px;
      font-weight: 500;
      margin: 0 0 12px 0;
    }

    dl {
      display: grid;
      grid-template-columns: auto 1fr;
      gap: 8px 16px;
      margin: 0;
      font-size: 13px;
    }

    dt {
      font-weight: 500;
      color: #5f6368;
    }

    dd {
      margin: 0;
    }

    code {
      background: #f8f9fa;
      padding: 2px 6px;
      border-radius: 3px;
      font-family: 'Roboto Mono', monospace;
      font-size: 12px;
    }

    .stream-actions {
      display: flex;
      gap: 8px;
    }

    button {
      padding: 8px 16px;
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

    .primary-button {
      background: #1a73e8;
      color: white;
      border-color: #1a73e8;
    }

    .primary-button:hover {
      background: #1765cc;
    }

    .danger-button {
      color: #d93025;
      border-color: #d93025;
    }

    .danger-button:hover {
      background: #fce8e6;
    }
  `;
}

customElements.define('streams-tab', StreamsTabElement);
