// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DataSipperObserverCallbackRouter, DataSipperPageHandlerFactory, DataSipperPageHandlerRemote, ExtractedData, NetworkRequestData} from './datasipper.mojom-webui.js';

console.log('DataSipper side panel loaded');

// Mojo handler
let pageHandler: DataSipperPageHandlerRemote|null = null;

// Store network requests
const networkRequests: NetworkRequestData[] = [];

// Initialize Mojo connection
function initializeMojo() {
  pageHandler = new DataSipperPageHandlerRemote();
  const factory = DataSipperPageHandlerFactory.getRemote();
  factory.createPageHandler(pageHandler.$.bindNewPipeAndPassReceiver());

  // Register observer using CallbackRouter
  const callbackRouter = new DataSipperObserverCallbackRouter();

  callbackRouter.onNetworkRequestCaptured.addListener(
      (request: NetworkRequestData) => {
    console.log('Network request captured:', request);
    networkRequests.push(request);

    // Keep only last 20 requests
    if (networkRequests.length > 20) {
      networkRequests.shift();
    }

    // Update UI
    displayNetworkRequests();
    updateStatus('Capturing', true);
  });

  callbackRouter.onDataExtracted.addListener((data: ExtractedData) => {
    console.log('Data extracted:', data);
    // TODO: Display extracted data
  });

  callbackRouter.onWebSocketMessage.addListener(
      (url: string, message: string, isOutgoing: boolean) => {
    console.log('WebSocket message:', {url, message, isOutgoing});
    // TODO: Display WebSocket messages
  });

  pageHandler.addObserver(callbackRouter.$.bindNewPipeAndPassRemote());

  // Notify backend that UI is ready to be shown
  pageHandler.showUI();
  console.log('DataSipper: Called showUI() and registered observer');
}

// Format timestamp
function formatTimestamp(timestampMs: bigint): string {
  const date = new Date(Number(timestampMs));
  return date.toLocaleTimeString();
}

// Format size
function formatSize(bytes: bigint): string {
  const num = Number(bytes);
  if (num < 1024) return `${num} B`;
  if (num < 1048576) return `${(num / 1024).toFixed(1)} KB`;
  return `${(num / 1048576).toFixed(1)} MB`;
}

// Display network requests
function displayNetworkRequests() {
  const container = document.getElementById('networkRequests');
  if (!container) return;

  // Clear existing content
  container.textContent = '';

  if (networkRequests.length === 0) {
    const empty = document.createElement('p');
    empty.className = 'empty-state';
    empty.textContent = 'Waiting for network activity...';
    container.appendChild(empty);
    return;
  }

  // Display requests in reverse order (newest first)
  for (let i = networkRequests.length - 1; i >= 0; i--) {
    const request = networkRequests[i]!;
    if (!request) continue;

    const item = document.createElement('div');
    item.className = 'network-item';

    // Status badge
    const statusClass = request.statusCode >= 200 && request.statusCode < 300
        ? 'success'
        : request.statusCode >= 400
        ? 'error'
        : 'info';

    // Build header
    const header = document.createElement('div');
    header.className = 'network-item-header';

    const methodBadge = document.createElement('span');
    methodBadge.className = `method-badge ${request.method}`;
    methodBadge.textContent = request.method;

    const statusBadge = document.createElement('span');
    statusBadge.className = `status-badge ${statusClass}`;
    statusBadge.textContent = String(request.statusCode);

    const typeBadge = document.createElement('span');
    typeBadge.className = 'type-badge';
    typeBadge.textContent = request.type;

    const timeSpan = document.createElement('span');
    timeSpan.className = 'time';
    timeSpan.textContent = formatTimestamp(request.timestampMs);

    header.appendChild(methodBadge);
    header.appendChild(statusBadge);
    header.appendChild(typeBadge);
    header.appendChild(timeSpan);

    // Build URL section
    const urlDiv = document.createElement('div');
    urlDiv.className = 'network-item-url';
    urlDiv.textContent = request.url;

    // Build details section
    const detailsDiv = document.createElement('div');
    detailsDiv.className = 'network-item-details';

    const sizeSpan = document.createElement('span');
    sizeSpan.textContent = `Size: ${formatSize(request.sizeBytes)}`;

    const durationSpan = document.createElement('span');
    durationSpan.textContent = `Duration: ${request.durationMs}ms`;

    detailsDiv.appendChild(sizeSpan);
    detailsDiv.appendChild(durationSpan);

    // Assemble item
    item.appendChild(header);
    item.appendChild(urlDiv);
    item.appendChild(detailsDiv);

    // Make item expandable to show details
    item.addEventListener('click', () => {
      showRequestDetails(request);
    });

    container.appendChild(item);
  }
}

// Current request being viewed
let currentRequest: NetworkRequestData|null = null;

// Show request details in modal
function showRequestDetails(request: NetworkRequestData) {
  currentRequest = request;
  const modal = document.getElementById('requestDetailsModal');
  if (!modal) return;

  // Populate general info
  populateGeneralInfo(request);

  // Populate headers
  populateHeaders('detailsRequestHeaders', request.requestHeaders);
  populateHeaders('detailsResponseHeaders', request.responseHeaders);

  // Populate bodies
  populateBody('detailsRequestBody', request.requestBody);
  populateBody('detailsResponseBody', request.responseBody);

  // Populate timing
  populateTiming(request);

  // Show modal
  modal.style.display = 'block';
}

// Populate general information
function populateGeneralInfo(request: NetworkRequestData) {
  const container = document.getElementById('detailsGeneral');
  if (!container) return;

  container.textContent = '';

  const fields = [
    {label: 'URL', value: request.url},
    {label: 'Method', value: request.method},
    {label: 'Status', value: `${request.statusCode} ${request.statusText}`},
    {label: 'Type', value: request.type},
    {label: 'Request ID', value: request.requestId},
  ];

  fields.forEach(field => {
    const row = document.createElement('div');
    row.className = 'timing-row';

    const label = document.createElement('span');
    label.className = 'timing-label';
    label.textContent = field.label + ':';

    const value = document.createElement('span');
    value.className = 'timing-value';
    value.textContent = field.value;

    row.appendChild(label);
    row.appendChild(value);
    container.appendChild(row);
  });
}

// Populate headers
function populateHeaders(containerId: string, headers: {[key: string]: string}) {
  const container = document.getElementById(containerId);
  if (!container) return;

  container.textContent = '';

  const headerKeys = Object.keys(headers);
  if (headerKeys.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'empty-content';
    empty.textContent = 'No headers';
    container.appendChild(empty);
    return;
  }

  headerKeys.forEach(key => {
    const row = document.createElement('div');
    row.className = 'header-row';

    const name = document.createElement('span');
    name.className = 'header-name';
    name.textContent = key + ': ';

    const value = document.createElement('span');
    value.className = 'header-value';
    value.textContent = headers[key] || '';

    row.appendChild(name);
    row.appendChild(value);
    container.appendChild(row);
  });
}

// Populate body content
function populateBody(containerId: string, body: string) {
  const container = document.getElementById(containerId);
  if (!container) return;

  if (!body || body.trim() === '') {
    container.textContent = '(empty)';
    container.className = 'details-content body-content empty-content';
    return;
  }

  container.className = 'details-content body-content';

  // Try to parse and prettify JSON
  try {
    const parsed = JSON.parse(body);
    container.textContent = JSON.stringify(parsed, null, 2);
  } catch (e) {
    // Not JSON, just display as-is
    container.textContent = body;
  }
}

// Populate timing information
function populateTiming(request: NetworkRequestData) {
  const container = document.getElementById('detailsTiming');
  if (!container) return;

  container.textContent = '';

  const timestamp = new Date(Number(request.timestampMs));

  const fields = [
    {label: 'Timestamp', value: timestamp.toLocaleString()},
    {label: 'Duration', value: `${request.durationMs} ms`},
    {label: 'Size', value: formatSize(request.sizeBytes)},
  ];

  fields.forEach(field => {
    const row = document.createElement('div');
    row.className = 'timing-row';

    const label = document.createElement('span');
    label.className = 'timing-label';
    label.textContent = field.label + ':';

    const value = document.createElement('span');
    value.className = 'timing-value';
    value.textContent = field.value;

    row.appendChild(label);
    row.appendChild(value);
    container.appendChild(row);
  });
}

// Close modal
function closeModal() {
  const modal = document.getElementById('requestDetailsModal');
  if (modal) {
    modal.style.display = 'none';
  }
  currentRequest = null;
}

// Copy content to clipboard
function copyToClipboard(type: string) {
  if (!currentRequest) return;

  let content = '';

  switch (type) {
    case 'request-headers':
      content = formatHeadersForCopy(currentRequest.requestHeaders);
      break;
    case 'response-headers':
      content = formatHeadersForCopy(currentRequest.responseHeaders);
      break;
    case 'request-body':
      content = currentRequest.requestBody;
      break;
    case 'response-body':
      content = currentRequest.responseBody;
      break;
  }

  if (!content) {
    console.log('No content to copy');
    return;
  }

  navigator.clipboard.writeText(content).then(() => {
    console.log('Copied to clipboard');
    // TODO: Show toast notification
  }).catch(err => {
    console.error('Failed to copy:', err);
  });
}

// Format headers for clipboard
function formatHeadersForCopy(headers: {[key: string]: string}): string {
  return Object.keys(headers)
      .map(key => `${key}: ${headers[key]}`)
      .join('\n');
}

// Update status indicator
function updateStatus(text: string, active: boolean) {
  const statusEl = document.getElementById('status');
  if (!statusEl) return;

  const indicator = statusEl.querySelector('.status-indicator') as HTMLElement;
  const textEl = statusEl.querySelector('span:last-child') as HTMLElement;

  if (indicator) {
    indicator.style.backgroundColor = active ? '#34a853' : '#999';
  }
  if (textEl) {
    textEl.textContent = text;
  }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
  console.log('DataSipper UI initialized');

  // Initialize Mojo connection
  initializeMojo();

  // Add event listeners for checkboxes
  document.getElementById('jsonEnabled')?.addEventListener('change', (e) => {
    const target = e.target as HTMLInputElement;
    console.log('JSON extraction:', target.checked);
  });

  document.getElementById('websocketsEnabled')?.addEventListener('change', (e) => {
    const target = e.target as HTMLInputElement;
    console.log('WebSocket monitoring:', target.checked);
  });

  document.getElementById('apiEnabled')?.addEventListener('change', (e) => {
    const target = e.target as HTMLInputElement;
    console.log('API tracking:', target.checked);
  });

  // Update initial page data
  updatePageData();

  // Add modal close button handler
  document.getElementById('closeModal')?.addEventListener('click', closeModal);

  // Close modal when clicking outside the modal content
  document.getElementById('requestDetailsModal')?.addEventListener('click', (e) => {
    const target = e.target as HTMLElement;
    if (target.id === 'requestDetailsModal') {
      closeModal();
    }
  });

  // Add copy button handlers
  document.querySelectorAll('.copy-button').forEach(button => {
    button.addEventListener('click', (e) => {
      const target = e.target as HTMLElement;
      const copyType = target.getAttribute('data-copy');
      if (copyType) {
        copyToClipboard(copyType);
      }
    });
  });
});

function updatePageData() {
  const pageDataEl = document.getElementById('pageData');
  if (pageDataEl) {
    // Clear existing content
    pageDataEl.textContent = '';

    // Create URL paragraph
    const urlP = document.createElement('p');
    const urlStrong = document.createElement('strong');
    urlStrong.textContent = 'URL: ';
    urlP.appendChild(urlStrong);
    urlP.appendChild(document.createTextNode(window.location.href));
    pageDataEl.appendChild(urlP);

    // Create status paragraph
    const statusP = document.createElement('p');
    const statusStrong = document.createElement('strong');
    statusStrong.textContent = 'Status: ';
    statusP.appendChild(statusStrong);
    statusP.appendChild(document.createTextNode('Monitoring...'));
    pageDataEl.appendChild(statusP);

    // Create ready message
    const readyP = document.createElement('p');
    const readyEm = document.createElement('em');
    readyEm.textContent = 'DataSipper is ready to extract and stream data';
    readyP.appendChild(readyEm);
    pageDataEl.appendChild(readyP);
  }
}
