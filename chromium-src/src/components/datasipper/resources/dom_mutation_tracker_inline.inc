// Auto-generated from dom_mutation_tracker.js
// DO NOT EDIT - Generated file

const char kDomMutationTrackerScript[] = R"JSCODE(
// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview DOM Mutation Stream Tracker
 *
 * Detects streaming data patterns in web pages by monitoring DOM mutations.
 * Analyzes timing, regularity, and data types to identify live data streams.
 */

(function() {
  'use strict';

  console.log('[DataSipper] DOM Stream Tracker loading...');

  // Configuration
  const CONFIG = {
    // Minimum number of updates before considering an element a stream
    MIN_UPDATES_FOR_STREAM: 3,

    // Minimum interval between updates (ms) - ignore faster changes
    MIN_UPDATE_INTERVAL: 100,

    // Maximum interval to still be considered a stream (ms)
    MAX_UPDATE_INTERVAL: 300000, // 5 minutes

    // Coefficient of variation threshold for "regular" streams
    REGULARITY_THRESHOLD: 0.2,

    // How often to check for and report streams (ms)
    REPORT_INTERVAL: 5000,

    // Maximum number of timestamps to keep per element
    MAX_TIMESTAMPS: 50,
  };

  // Data type enum matching C++ StreamType
  const DataType = {
    UNKNOWN: 'UNKNOWN',
    NUMERIC: 'NUMERIC',
    TEXT: 'TEXT',
    JSON: 'JSON',
    XML: 'XML',
    BINARY: 'BINARY'
  };

  /**
   * Tracked element data
   */
  class TrackedElement {
    constructor(selector) {
      this.selector = selector;
      this.updateTimestamps = [];
      this.values = [];
      this.updateCount = 0;
      this.firstSeen = Date.now();
      this.lastSeen = Date.now();
      this.reported = false;
    }

    /**
     * Record an update to this element
     */
    recordUpdate(value) {
      const now = Date.now();

      // Ignore updates that are too fast (likely not real data changes)
      if (this.updateTimestamps.length > 0) {
        const timeSinceLastUpdate = now - this.updateTimestamps[this.updateTimestamps.length - 1];
        if (timeSinceLastUpdate < CONFIG.MIN_UPDATE_INTERVAL) {
          return;
        }
      }

      this.updateTimestamps.push(now);
      this.values.push(value);
      this.updateCount++;
      this.lastSeen = now;

      // Keep only recent timestamps to avoid memory issues
      if (this.updateTimestamps.length > CONFIG.MAX_TIMESTAMPS) {
        this.updateTimestamps.shift();
        this.values.shift();
      }
    }

    /**
     * Calculate timing statistics
     */
    getTimingStats() {
      if (this.updateTimestamps.length < 2) {
        return null;
      }

      const intervals = [];
      for (let i = 1; i < this.updateTimestamps.length; i++) {
        intervals.push(this.updateTimestamps[i] - this.updateTimestamps[i - 1]);
      }

      const avgInterval = intervals.reduce((a, b) => a + b, 0) / intervals.length;

      // Calculate standard deviation
      const squareDiffs = intervals.map(interval => Math.pow(interval - avgInterval, 2));
      const avgSquareDiff = squareDiffs.reduce((a, b) => a + b, 0) / squareDiffs.length;
      const stdDev = Math.sqrt(avgSquareDiff);

      // Coefficient of variation (normalized standard deviation)
      const coefficientOfVariation = avgInterval > 0 ? stdDev / avgInterval : 0;

      // Is this a regular pattern?
      const isRegular = coefficientOfVariation < CONFIG.REGULARITY_THRESHOLD;

      return {
        avgIntervalMs: avgInterval,
        stdDeviation: stdDev,
        coefficientOfVariation: coefficientOfVariation,
        isRegular: isRegular
      };
    }

    /**
     * Classify the data type based on values
     */
    classifyDataType() {
      if (this.values.length === 0) {
        return DataType.UNKNOWN;
      }

      // Check last few values
      const recentValues = this.values.slice(-5);

      // Count how many match each pattern
      let numericCount = 0;
      let jsonCount = 0;
      let xmlCount = 0;

      for (const value of recentValues) {
        const trimmed = value.trim();

        // Check for JSON
        if ((trimmed.startsWith('{') && trimmed.endsWith('}')) ||
            (trimmed.startsWith('[') && trimmed.endsWith(']'))) {
          try {
            JSON.parse(trimmed);
            jsonCount++;
            continue;
          } catch (e) {
            // Not valid JSON
          }
        }

        // Check for XML
        if (trimmed.startsWith('<') && trimmed.endsWith('>')) {
          xmlCount++;
          continue;
        }

        // Check for numeric (including currency, percentages, etc.)
        const numericPattern = /^[\$€£¥]?\s*-?\d+([.,]\d+)*\s*[%]?$/;
        if (numericPattern.test(trimmed)) {
          numericCount++;
          continue;
        }
      }

      // Determine primary type
      const total = recentValues.length;
      if (jsonCount / total > 0.5) return DataType.JSON;
      if (xmlCount / total > 0.5) return DataType.XML;
      if (numericCount / total > 0.5) return DataType.NUMERIC;

      return DataType.TEXT;
    }

    /**
     * Check if this element qualifies as a stream
     */
    isStream() {
      if (this.updateCount < CONFIG.MIN_UPDATES_FOR_STREAM) {
        return false;
      }

      const stats = this.getTimingStats();
      if (!stats) {
        return false;
      }

      // Check if average interval is within acceptable range
      if (stats.avgIntervalMs < CONFIG.MIN_UPDATE_INTERVAL ||
          stats.avgIntervalMs > CONFIG.MAX_UPDATE_INTERVAL) {
        return false;
      }

      return true;
    }

    /**
     * Get stream definition to report to C++
     */
    toStreamDefinition() {
      const stats = this.getTimingStats();
      const dataType = this.classifyDataType();
      const currentValue = this.values.length > 0 ? this.values[this.values.length - 1] : '';

      return {
        selector: this.selector,
        updateCount: this.updateCount,
        avgIntervalMs: stats ? stats.avgIntervalMs : 0,
        stdDeviation: stats ? stats.stdDeviation : 0,
        coefficientOfVariation: stats ? stats.coefficientOfVariation : 0,
        isRegular: stats ? stats.isRegular : false,
        dataType: dataType,
        currentValue: currentValue.substring(0, 100), // Limit length
        firstSeen: this.firstSeen,
        lastSeen: this.lastSeen
      };
    }
  }

  /**
   * Main stream tracker class
   */
  class StreamTracker {
    constructor() {
      this.trackedElements = new Map(); // selector -> TrackedElement
      this.observer = null;
      this.reportTimer = null;
      this.elementToSelector = new WeakMap(); // element -> selector
      this.selectorCounter = 0;
      this.messageQueue = []; // Queue of messages to send to C++
    }

    /**
     * Start tracking
     */
    start() {
      console.log('[DataSipper] Starting DOM stream tracking');

      // Set up MutationObserver
      this.observer = new MutationObserver((mutations) => {
        this.handleMutations(mutations);
      });

      // Observe the entire document
      this.observer.observe(document.body, {
        childList: true,
        subtree: true,
        characterData: true,
        characterDataOldValue: true
      });

      // Start periodic reporting
      this.reportTimer = setInterval(() => {
        this.reportStreams();
      }, CONFIG.REPORT_INTERVAL);

      // Do an initial scan
      this.scanPage();

      console.log('[DataSipper] Stream tracking started');
    }

    /**
     * Stop tracking
     */
    stop() {
      if (this.observer) {
        this.observer.disconnect();
        this.observer = null;
      }

      if (this.reportTimer) {
        clearInterval(this.reportTimer);
        this.reportTimer = null;
      }

      console.log('[DataSipper] Stream tracking stopped');
    }

    /**
     * Scan page for potential stream elements
     */
    scanPage() {
      // Look for elements with IDs that suggest live data
      const liveDataPatterns = [
        'price', 'stock', 'ticker', 'quote', 'rate', 'score', 'count',
        'time', 'date', 'status', 'value', 'amount', 'balance', 'total',
        'live', 'update', 'current', 'latest'
      ];

      let foundCount = 0;

      // Scan by ID
      for (const pattern of liveDataPatterns) {
        const elements = document.querySelectorAll(`[id*="${pattern}" i]`);
        elements.forEach(el => {
          if (this.shouldTrackElement(el)) {
            this.trackElement(el);
            foundCount++;
          }
        });
      }

      // Scan by common class names
      const commonClasses = [
        '.price', '.value', '.count', '.score', '.status', '.ticker',
        '.live-data', '.real-time', '.update'
      ];

      for (const className of commonClasses) {
        const elements = document.querySelectorAll(className);
        elements.forEach(el => {
          if (this.shouldTrackElement(el)) {
            this.trackElement(el);
            foundCount++;
          }
        });
      }

      console.log(`[DataSipper] Initial scan found ${foundCount} potential stream elements`);
      return foundCount;
    }

    /**
     * Check if element should be tracked
     */
    shouldTrackElement(element) {
      // Skip if already tracked
      if (this.elementToSelector.has(element)) {
        return false;
      }

      // Skip if no text content
      const text = this.getElementText(element);
      if (!text || text.length === 0) {
        return false;
      }

      // Skip if too large (likely not a simple data field)
      if (text.length > 500) {
        return false;
      }

      // Skip script and style tags
      const tagName = element.tagName.toLowerCase();
      if (tagName === 'script' || tagName === 'style') {
        return false;
      }

      return true;
    }

    /**
     * Start tracking an element
     */
    trackElement(element) {
      const selector = this.getElementSelector(element);

      if (!this.trackedElements.has(selector)) {
        this.trackedElements.set(selector, new TrackedElement(selector));
        this.elementToSelector.set(element, selector);

        // Record initial value
        const text = this.getElementText(element);
        const tracked = this.trackedElements.get(selector);
        tracked.recordUpdate(text);
      }
    }

    /**
     * Generate a unique selector for an element
     */
    getElementSelector(element) {
      // Try ID first
      if (element.id) {
        return `#${element.id}`;
      }

      // Try combination of tag and class
      if (element.className && typeof element.className === 'string') {
        const classes = element.className.split(' ').filter(c => c.length > 0);
        if (classes.length > 0) {
          return `${element.tagName.toLowerCase()}.${classes[0]}`;
        }
      }

      // Try nth-child selector
      const parent = element.parentElement;
      if (parent) {
        const siblings = Array.from(parent.children);
        const index = siblings.indexOf(element);
        if (index >= 0) {
          const parentSelector = parent.id ? `#${parent.id}` : parent.tagName.toLowerCase();
          return `${parentSelector} > ${element.tagName.toLowerCase()}:nth-child(${index + 1})`;
        }
      }

      // Fallback: generate unique selector
      return `datasipper-stream-${++this.selectorCounter}`;
    }

    /**
     * Get text content from element
     */
    getElementText(element) {
      // For input elements, use value
      if (element.tagName === 'INPUT' || element.tagName === 'TEXTAREA') {
        return element.value || '';
      }

      // For other elements, use textContent
      return (element.textContent || '').trim();
    }

    /**
     * Handle DOM mutations
     */
    handleMutations(mutations) {
      for (const mutation of mutations) {
        if (mutation.type === 'characterData') {
          // Text content changed
          const element = mutation.target.parentElement;
          if (element && this.shouldTrackElement(element)) {
            const selector = this.elementToSelector.get(element);
            if (selector) {
              // Already tracked, record update
              const tracked = this.trackedElements.get(selector);
              if (tracked) {
                const newText = this.getElementText(element);
                if (newText !== tracked.values[tracked.values.length - 1]) {
                  tracked.recordUpdate(newText);
                }
              }
            } else {
              // Not yet tracked, start tracking
              this.trackElement(element);
            }
          }
        } else if (mutation.type === 'childList') {
          // Check if content of existing tracked elements changed
          mutation.addedNodes.forEach(node => {
            if (node.nodeType === Node.ELEMENT_NODE) {
              // Check if this is a tracked element or contains tracked elements
              if (this.elementToSelector.has(node)) {
                const selector = this.elementToSelector.get(node);
                const tracked = this.trackedElements.get(selector);
                if (tracked) {
                  const newText = this.getElementText(node);
                  if (newText !== tracked.values[tracked.values.length - 1]) {
                    tracked.recordUpdate(newText);
                  }
                }
              }
            }
          });
        }
      }
    }

    /**
     * Report detected streams to C++
     */
    reportStreams() {
      let reportedCount = 0;

      for (const [selector, tracked] of this.trackedElements) {
        // Only report streams that meet criteria and haven't been reported
        if (tracked.isStream() && !tracked.reported) {
          const streamDef = tracked.toStreamDefinition();

          // Add to message queue for C++ to poll
          this.messageQueue.push({
            type: 'DATASIPPER_STREAM_DETECTED',
            stream: streamDef
          });

          tracked.reported = true;
          reportedCount++;

          console.log(`[DataSipper] Stream detected: ${selector}`, streamDef);
        }
      }

      if (reportedCount > 0) {
        console.log(`[DataSipper] Reported ${reportedCount} new streams`);
      }
    }

    /**
     * Get pending messages (called by C++ polling)
     */
    getMessages() {
      const messages = this.messageQueue.slice(); // Copy messages
      this.messageQueue = []; // Clear queue
      return messages;
    }

    /**
     * Get statistics for debugging
     */
    getStats() {
      const totalTracked = this.trackedElements.size;
      const streams = Array.from(this.trackedElements.values()).filter(t => t.isStream());
      const streamCount = streams.length;

      return {
        totalTracked,
        streamCount,
        streams: streams.map(s => s.toStreamDefinition())
      };
    }
  }

  // Create global tracker instance
  const tracker = new StreamTracker();

  // Expose to window for C++ access
  window.__dataSipperStreamTracker = {
    start: () => tracker.start(),
    stop: () => tracker.stop(),
    scanPage: () => tracker.scanPage(),
    getStats: () => tracker.getStats(),
    getMessages: () => tracker.getMessages()
  };

  // Auto-start when DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
      tracker.start();
    });
  } else {
    // DOM already loaded
    tracker.start();
  }

  // Signal that tracker is ready
  tracker.messageQueue.push({
    type: 'DATASIPPER_TRACKER_READY'
  });

  console.log('[DataSipper] DOM Stream Tracker ready');
})();
)JSCODE";
