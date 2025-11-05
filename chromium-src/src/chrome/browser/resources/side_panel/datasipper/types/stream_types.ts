// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TypeScript type definitions for DataSipper stream detection.
 * These types match the Mojo interface definitions in datasipper.mojom.
 */

// Types of data streams
export enum StreamType {
  DOM_MUTATION = 0,
  NETWORK_PATTERN = 1,
  WEBSOCKET_STREAM = 2,
}

// Data type classification
export enum DataType {
  UNKNOWN = 0,
  JSON = 1,
  XML = 2,
  CSV = 3,
  TEXT = 4,
  BINARY = 5,
}

// Timing pattern information for a stream
export interface StreamTimingInfo {
  avgIntervalMs: number;
  stdDeviation: number;
  isRegular: boolean;
}

// Complete stream information
export interface StreamInfo {
  streamId: string;
  label: string;
  type: StreamType;
  sourcePattern: string;
  url: string;
  firstSeen: bigint;  // Unix timestamp in milliseconds
  lastSeen: bigint;
  timing: StreamTimingInfo;
  isActive: boolean;
  eventCount: number;
  dataType: DataType;
}

// Individual stream event
export interface StreamEventInfo {
  streamId: string;
  timestamp: bigint;
  eventType: string;
  data: string;  // JSON string
}

// Stream subscription
export interface StreamSubscription {
  subscriptionId: string;
  streamId: string;
}

// UI-specific filter configuration
export interface StreamFilter {
  type?: StreamType;
  activeOnly: boolean;
  searchQuery: string;
}

// Aggregate stream statistics
export interface StreamStats {
  totalStreams: number;
  activeStreams: number;
  totalEvents: number;
  avgInterval: number;
}

// Helper functions for formatting

export function getStreamTypeName(type: StreamType): string {
  switch (type) {
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

export function getStreamTypeIcon(type: StreamType): string {
  switch (type) {
    case StreamType.DOM_MUTATION:
      return '=Ä';
    case StreamType.NETWORK_PATTERN:
      return '<';
    case StreamType.WEBSOCKET_STREAM:
      return '¡';
    default:
      return '=Ê';
  }
}

export function formatInterval(ms: number): string {
  if (ms < 1000) {
    return `${Math.round(ms)}ms`;
  } else if (ms < 60000) {
    return `${Math.round(ms / 1000)}s`;
  } else {
    return `${Math.round(ms / 60000)}min`;
  }
}

export function formatTimestamp(timestamp: bigint): string {
  const now = Date.now();
  const diff = now - Number(timestamp);
  const seconds = Math.floor(diff / 1000);

  if (seconds < 60) return `${seconds}s ago`;
  if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
  if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ago`;
  return `${Math.floor(seconds / 86400)}d ago`;
}

export function formatTimeOfDay(timestamp: bigint): string {
  const date = new Date(Number(timestamp));
  return date.toLocaleTimeString('en-US', { hour12: false });
}

export function formatEventData(dataStr: string): string {
  try {
    const data = JSON.parse(dataStr);
    return JSON.stringify(data, null, 2);
  } catch {
    return dataStr;
  }
}
