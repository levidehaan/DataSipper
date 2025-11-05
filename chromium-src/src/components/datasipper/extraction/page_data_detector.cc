// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/extraction/page_data_detector.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/datasipper/extraction/csv_table_extractor.h"
#include "components/datasipper/extraction/html_structure_extractor.h"
#include "components/datasipper/extraction/json_extractor.h"
#include "components/datasipper/extraction/jsonld_extractor.h"
#include "components/datasipper/streaming/dom_stream_tracker.h"
#include "components/datasipper/streaming/stream_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace datasipper {

PageDataDetector::PageDataDetector(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      html_extractor_(std::make_unique<HtmlStructureExtractor>()),
      json_extractor_(std::make_unique<JsonExtractor>()),
      jsonld_extractor_(std::make_unique<JsonLdExtractor>()),
      csv_extractor_(std::make_unique<CsvTableExtractor>()),
      stream_tracker_(std::make_unique<DOMStreamTracker>(web_contents)) {
  DCHECK(web_contents);
  LOG(INFO) << "PageDataDetector created with stream tracking support";
}

PageDataDetector::~PageDataDetector() = default;

PageExtractionResult PageDataDetector::DetectAllDataSources() {
  PageExtractionResult result;

  if (!web_contents()) {
    result.errors.push_back("WebContents not available");
    return result;
  }

  // Get the page HTML by executing JavaScript
  // In a real implementation, we'd use DevTools protocol or RenderFrameHost
  // For now, we'll use a simplified approach

  // Execute JavaScript to get the HTML content
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  if (!main_frame) {
    result.errors.push_back("Main frame not available");
    return result;
  }

  // Note: This is a simplified version. In production, we would:
  // 1. Use ExecuteJavaScript to get document.documentElement.outerHTML
  // 2. Process the result asynchronously
  // For now, we'll demonstrate the extraction logic assuming we have the HTML

  result.url = web_contents()->GetLastCommittedURL().spec();
  result.scanned_at = base::Time::Now();

  // Note: This needs to be async in real implementation
  // For now, return structure showing what would be extracted

  return result;
}

void PageDataDetector::EnableLiveTracking(DataSourcesCallback callback) {
  live_tracking_enabled_ = true;
  data_sources_callback_ = std::move(callback);

  // Perform initial detection
  RefreshDetection();
}

void PageDataDetector::DisableLiveTracking() {
  live_tracking_enabled_ = false;
  data_sources_callback_.Reset();
}

void PageDataDetector::RefreshDetection() {
  if (!web_contents()) {
    return;
  }

  // Execute JavaScript to get the page HTML
  ExecuteExtractionScript();
}

void PageDataDetector::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  if (live_tracking_enabled_) {
    RefreshDetection();
  }
}

void PageDataDetector::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  if (live_tracking_enabled_) {
    RefreshDetection();
  }
}

void PageDataDetector::ExecuteExtractionScript() {
  if (!web_contents()) {
    return;
  }

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  if (!main_frame) {
    return;
  }

  // JavaScript to extract page HTML
  const char16_t kExtractionScript[] = uR"(
    (function() {
      return {
        html: document.documentElement.outerHTML,
        url: window.location.href,
        title: document.title
      };
    })();
  )";

  main_frame->ExecuteJavaScriptForTests(
      kExtractionScript,
      base::BindOnce(&PageDataDetector::OnExtractionComplete,
                     weak_factory_.GetWeakPtr()),
      0 /* ISOLATED_WORLD_ID_GLOBAL */);
}

void PageDataDetector::OnExtractionComplete(base::Value result) {
  if (!result.is_dict()) {
    LOG(WARNING) << "PageDataDetector: Invalid extraction result";
    return;
  }

  const base::Value::Dict& dict = result.GetDict();
  const std::string* html = dict.FindString("html");

  if (!html || html->empty()) {
    LOG(WARNING) << "PageDataDetector: No HTML content extracted";
    return;
  }

  // Clear previous results
  last_detected_sources_.clear();

  // Extract data using all extractors
  std::vector<ExtractedDataSource> html_sources = ExtractHtmlStructures();
  std::vector<ExtractedDataSource> json_sources = ExtractJsonObjects();
  std::vector<ExtractedDataSource> jsonld_sources = ExtractJsonLd();
  std::vector<ExtractedDataSource> csv_sources = ExtractCsvTables();

  // Combine all results
  last_detected_sources_.insert(last_detected_sources_.end(),
                                html_sources.begin(), html_sources.end());
  last_detected_sources_.insert(last_detected_sources_.end(),
                                json_sources.begin(), json_sources.end());
  last_detected_sources_.insert(last_detected_sources_.end(),
                                jsonld_sources.begin(), jsonld_sources.end());
  last_detected_sources_.insert(last_detected_sources_.end(),
                                csv_sources.begin(), csv_sources.end());

  // Notify observers
  NotifyDataSourcesChanged();
}

std::vector<ExtractedDataSource> PageDataDetector::ExtractHtmlStructures() {
  if (!web_contents()) {
    return {};
  }

  // In real implementation, we'd pass the actual HTML
  // For now, return empty to show structure
  std::vector<ExtractedDataSource> sources;

  // This would call:
  // std::vector<ExtractedDataSource> tables = html_extractor_->ExtractTables(html);
  // std::vector<ExtractedDataSource> lists = html_extractor_->ExtractLists(html);
  // std::vector<ExtractedDataSource> forms = html_extractor_->ExtractForms(html);

  return sources;
}

std::vector<ExtractedDataSource> PageDataDetector::ExtractJsonObjects() {
  if (!web_contents()) {
    return {};
  }

  std::vector<ExtractedDataSource> sources;

  // This would call:
  // std::vector<ExtractedDataSource> script_tags =
  //     json_extractor_->ExtractFromScriptTags(html);
  // std::vector<ExtractedDataSource> data_attrs =
  //     json_extractor_->ExtractFromDataAttributes(html);

  return sources;
}

std::vector<ExtractedDataSource> PageDataDetector::ExtractJsonLd() {
  if (!web_contents()) {
    return {};
  }

  std::vector<ExtractedDataSource> sources;

  // This would call:
  // sources = jsonld_extractor_->ExtractFromHtml(html);

  return sources;
}

std::vector<ExtractedDataSource> PageDataDetector::ExtractCsvTables() {
  if (!web_contents()) {
    return {};
  }

  std::vector<ExtractedDataSource> sources;

  // CSV detection would look for <pre> tags or text nodes containing CSV data
  // This requires more complex logic to identify CSV patterns in the page

  return sources;
}

void PageDataDetector::NotifyDataSourcesChanged() {
  if (live_tracking_enabled_ && data_sources_callback_) {
    data_sources_callback_.Run(last_detected_sources_);
  }
}

void PageDataDetector::EnableStreamTracking(StreamDetectedCallback callback) {
  stream_detected_callback_ = std::move(callback);

  if (!stream_tracker_) {
    LOG(WARNING) << "Stream tracker not available";
    return;
  }

  LOG(INFO) << "Enabling stream tracking";

  // Enable the DOM stream tracker with our callback
  stream_tracker_->EnableTracking(
      base::BindRepeating(&PageDataDetector::OnStreamDetected,
                          weak_factory_.GetWeakPtr()));
}

void PageDataDetector::DisableStreamTracking() {
  if (stream_tracker_) {
    stream_tracker_->DisableTracking();
  }
  stream_detected_callback_.Reset();

  LOG(INFO) << "Stream tracking disabled";
}

std::vector<StreamDefinition> PageDataDetector::GetActiveStreams() const {
  if (!stream_tracker_) {
    return {};
  }
  return stream_tracker_->GetActiveStreams();
}

void PageDataDetector::ScanForStreams() {
  if (!stream_tracker_) {
    LOG(WARNING) << "Stream tracker not available";
    return;
  }

  LOG(INFO) << "Manually scanning page for streams";
  stream_tracker_->ScanPage();
}

void PageDataDetector::OnStreamDetected(const StreamDefinition& stream) {
  LOG(INFO) << "Stream detected: " << stream.label
            << " (type: " << static_cast<int>(stream.type)
            << ", interval: " << stream.timing.avg_interval_ms << "ms)";

  // Forward to callback if set
  if (stream_detected_callback_) {
    stream_detected_callback_.Run(stream);
  }
}

}  // namespace datasipper
