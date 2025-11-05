// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * DOM Mutation Tracker - Detects streams of data updates in the DOM
 *
 * This script is injected into web pages to monitor DOM mutations and identify
 * elements that update regularly (e.g., stock prices, scores, counters).
 *
 * Detection Algorithm:
 * 1. Use MutationObserver to track text content and attribute changes
 * 2. Build temporal fingerprints for each element
 * 3. Detect patterns (same element changing ≥3 times)
 * 4. Analyze timing regularity (coefficient of variation < 0.2)
 * 5. Classify data type (numeric, text, JSON)
 * 6. Report detected streams to C++ via postMessage
 */

(function() {
  'use strict';

  // Configuration
  const CONFIG = {
    MIN_UPDATES_FOR_STREAM: 3,      // Minimum updates before classifying as stream
    MAX_TRACKED_ELEMENTS: 500,      // Max elements to track simultaneously
    CLEANUP_INTERVAL_MS: 30000,     // Cleanup inactive trackers every 30s
    INACTIVE_THRESHOLD_MS: 300000,  // Remove trackers inactive for 5 minutes
    THROTTLE_MS: 100,               // Throttle mutation processing
    REGULARITY_THRESHOLD: 0.25,     // CV threshold for regular patterns
  };

  // Element tracker
  class ElementTracker {
    constructor(element) {
      this.element = element;
      this.selector = getElementSelector(element);
      this.updates = [];
      this.lastValue = '';
      this.firstSeen = Date.now();
      this.lastSeen = Date.now();
      this.updateCount = 0;
      this.streamReported = false;
    }

    recordUpdate(value) {
      const now = Date.now();

      // Skip if value hasn't changed
      if (value === this.lastValue) {
        return false;
      }

      this.updates.push({
        timestamp: now,
        value: value,
        delta: now - this.lastSeen
      });

      this.lastValue = value;
      this.lastSeen = now;
      this.updateCount++;

      // Keep only last 20 updates for analysis
      if (this.updates.length > 20) {
        this.updates.shift();
      }

      return true;
    }

    analyzePattern() {
      if (this.updateCount < CONFIG.MIN_UPDATES_FOR_STREAM) {
        return null;
      }

      if (this.streamReported) {
        return null;
      }

      // Calculate timing statistics
      const deltas = this.updates.slice(1).map(u => u.delta);
      if (deltas.length === 0) {
        return null;
      }

      const avgInterval = deltas.reduce((a, b) => a + b, 0) / deltas.length;
      const variance = deltas.reduce((sum, d) => sum + Math.pow(d - avgInterval, 2), 0) / deltas.length;
      const stdDev = Math.sqrt(variance);
      const coefficientOfVariation = stdDev / avgInterval;

      // Check if pattern is regular
      const isRegular = coefficientOfVariation < CONFIG.REGULARITY_THRESHOLD;

      // Classify data type
      const dataType = classifyDataType(this.updates.map(u => u.value));

      // Only report if pattern is sufficiently regular
      if (!isRegular && this.updateCount < 10) {
        return null;
      }

      // Build stream definition
      const stream = {
        selector: this.selector,
        updateCount: this.updateCount,
        avgIntervalMs: avgInterval,
        stdDeviation: stdDev,
        isRegular: isRegular,
        coefficientOfVariation: coefficientOfVariation,
        dataType: dataType,
        firstSeen: this.firstSeen,
        lastSeen: this.lastSeen,
        currentValue: this.lastValue,
        sampleValues: this.updates.slice(-5).map(u => u.value)
      };

      this.streamReported = true;
      return stream;
    }

    isStale() {
      return (Date.now() - this.lastSeen) > CONFIG.INACTIVE_THRESHOLD_MS;
    }
  }

  // Global state
  const trackers = new Map();
  let observer = null;
  let throttleTimer = null;
  let pendingMutations = [];

  // Generate CSS selector for an element
  function getElementSelector(element) {
    if (!element || element === document) {
      return '';
    }

    // Try ID first
    if (element.id) {
      return `#${element.id}`;
    }

    // Build path from classes and tag
    const path = [];
    let current = element;

    while (current && current !== document.body && path.length < 5) {
      let selector = current.tagName.toLowerCase();

      if (current.className && typeof current.className === 'string') {
        const classes = current.className.trim().split(/\s+/).slice(0, 2);
        if (classes.length > 0 && classes[0]) {
          selector += '.' + classes.join('.');
        }
      }

      // Add data attributes for better identification
      const dataAttrs = Array.from(current.attributes || [])
        .filter(attr => attr.name.startsWith('data-') && attr.value)
        .slice(0, 1);

      if (dataAttrs.length > 0) {
        selector += `[${dataAttrs[0].name}="${dataAttrs[0].value}"]`;
      }

      path.unshift(selector);
      current = current.parentElement;
    }

    return path.join(' > ');
  }

  // Classify data type based on values
  function classifyDataType(values) {
    if (values.length === 0) {
      return 'UNKNOWN';
    }

    let numericCount = 0;
    let jsonCount = 0;

    for (const value of values) {
      // Check if numeric (including currency, percentages)
      const numericPattern = /^[\d\s,.$€£¥%+-]+$/;
      if (numericPattern.test(value)) {
        numericCount++;
      }

      // Check if JSON
      if (value.trim().startsWith('{') || value.trim().startsWith('[')) {
        try {
          JSON.parse(value);
          jsonCount++;
        } catch (e) {
          // Not valid JSON
        }
      }
    }

    const total = values.length;

    if (numericCount / total > 0.8) {
      return 'NUMERIC';
    }

    if (jsonCount / total > 0.5) {
      return 'JSON';
    }

    return 'TEXT';
  }

  // Get element's text content
  function getElementContent(element) {
    // For input elements
    if (element.tagName === 'INPUT' || element.tagName === 'TEXTAREA') {
      return element.value;
    }

    // For elements with text content
    if (element.textContent) {
      return element.textContent.trim();
    }

    return '';
  }

  // Process mutations (throttled)
  function processMutations(mutations) {
    pendingMutations.push(...mutations);

    if (throttleTimer) {
      return;
    }

    throttleTimer = setTimeout(() => {
      const toProcess = pendingMutations.splice(0);
      throttleTimer = null;

      for (const mutation of toProcess) {
        handleMutation(mutation);
      }
    }, CONFIG.THROTTLE_MS);
  }

  // Handle a single mutation
  function handleMutation(mutation) {
    const target = mutation.target;

    // Skip non-element nodes
    if (target.nodeType !== Node.ELEMENT_NODE) {
      // Check if parent is an element
      if (mutation.target.parentElement) {
        handleElementChange(mutation.target.parentElement);
      }
      return;
    }

    handleElementChange(target);
  }

  // Handle element change
  function handleElementChange(element) {
    // Skip script, style, and hidden elements
    if (!element ||
        element.tagName === 'SCRIPT' ||
        element.tagName === 'STYLE' ||
        element.offsetParent === null) {
      return;
    }

    const content = getElementContent(element);

    // Skip empty content
    if (!content || content.length === 0) {
      return;
    }

    // Get or create tracker
    let tracker = trackers.get(element);
    if (!tracker) {
      // Limit number of tracked elements
      if (trackers.size >= CONFIG.MAX_TRACKED_ELEMENTS) {
        return;
      }

      tracker = new ElementTracker(element);
      trackers.set(element, tracker);
    }

    // Record update
    const changed = tracker.recordUpdate(content);

    if (changed) {
      // Analyze for stream pattern
      const stream = tracker.analyzePattern();
      if (stream) {
        reportStream(stream);
      }
    }
  }

  // Report detected stream to C++
  function reportStream(stream) {
    // Post message to C++ code
    const message = {
      type: 'DATASIPPER_STREAM_DETECTED',
      stream: stream,
      url: window.location.href,
      title: document.title
    };

    // Send via postMessage (will be intercepted by C++)
    window.postMessage(message, '*');

    console.log('[DataSipper] Stream detected:', stream.selector);
  }

  // Cleanup stale trackers
  function cleanup() {
    for (const [element, tracker] of trackers) {
      if (tracker.isStale()) {
        trackers.delete(element);
      }
    }
  }

  // Initialize observer
  function initialize() {
    console.log('[DataSipper] DOM Mutation Tracker initialized');

    // Create MutationObserver
    observer = new MutationObserver(processMutations);

    // Observe the entire document
    observer.observe(document.body, {
      childList: true,
      subtree: true,
      characterData: true,
      characterDataOldValue: false,
      attributes: true,
      attributeFilter: ['value', 'data-value', 'aria-valuenow'],
      attributeOldValue: false
    });

    // Periodic cleanup
    setInterval(cleanup, CONFIG.CLEANUP_INTERVAL_MS);

    // Report ready status
    window.postMessage({
      type: 'DATASIPPER_TRACKER_READY',
      config: CONFIG
    }, '*');
  }

  // Manual scan trigger
  function scanPage() {
    console.log('[DataSipper] Manual page scan triggered');

    // Reset all trackers
    trackers.clear();

    // Scan all potentially interesting elements
    const selectors = [
      '[data-price]',
      '[data-value]',
      '[data-count]',
      '.price',
      '.count',
      '.score',
      '.value',
      '[aria-valuenow]'
    ];

    selectors.forEach(selector => {
      try {
        document.querySelectorAll(selector).forEach(element => {
          const content = getElementContent(element);
          if (content) {
            const tracker = new ElementTracker(element);
            tracker.recordUpdate(content);
            trackers.set(element, tracker);
          }
        });
      } catch (e) {
        // Invalid selector, skip
      }
    });

    return trackers.size;
  }

  // Expose API for manual control
  window.__dataSipperStreamTracker = {
    scanPage: scanPage,
    getTrackedCount: () => trackers.size,
    getStreams: () => {
      const streams = [];
      for (const tracker of trackers.values()) {
        if (tracker.updateCount >= CONFIG.MIN_UPDATES_FOR_STREAM) {
          streams.push(tracker.analyzePattern());
        }
      }
      return streams.filter(s => s !== null);
    }
  };

  // Start tracking when DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initialize);
  } else {
    initialize();
  }

})();
