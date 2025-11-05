// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_EXTRACTION_PAGE_DATA_DETECTOR_H_
#define COMPONENTS_DATASIPPER_EXTRACTION_PAGE_DATA_DETECTOR_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/datasipper/extraction/extraction_result.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace datasipper {

class HtmlStructureExtractor;
class JsonExtractor;
class JsonLdExtractor;
class CsvTableExtractor;
class DOMStreamTracker;
struct StreamDefinition;

// Main orchestrator for detecting and extracting structured data from web pages
class PageDataDetector : public content::WebContentsObserver {
 public:
  using DataSourcesCallback =
      base::RepeatingCallback<void(const std::vector<ExtractedDataSource>&)>;
  using StreamDetectedCallback =
      base::RepeatingCallback<void(const StreamDefinition&)>;

  explicit PageDataDetector(content::WebContents* web_contents);
  ~PageDataDetector() override;

  PageDataDetector(const PageDataDetector&) = delete;
  PageDataDetector& operator=(const PageDataDetector&) = delete;

  // Perform a one-time scan of the current page
  PageExtractionResult DetectAllDataSources();

  // Enable live tracking - callback is invoked whenever new data is detected
  void EnableLiveTracking(DataSourcesCallback callback);
  void DisableLiveTracking();

  // Get the last detected data sources
  const std::vector<ExtractedDataSource>& GetLastDetectedSources() const {
    return last_detected_sources_;
  }

  // Force a re-scan of the page
  void RefreshDetection();

  // Stream tracking methods
  void EnableStreamTracking(StreamDetectedCallback callback);
  void DisableStreamTracking();
  std::vector<StreamDefinition> GetActiveStreams() const;
  void ScanForStreams();

  // WebContentsObserver implementation
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;

 private:
  // Execute JavaScript to extract data
  void ExecuteExtractionScript();

  // Process extraction results from JavaScript
  void OnExtractionComplete(base::Value result);

  // Individual extractor methods
  std::vector<ExtractedDataSource> ExtractHtmlStructures();
  std::vector<ExtractedDataSource> ExtractJsonObjects();
  std::vector<ExtractedDataSource> ExtractJsonLd();
  std::vector<ExtractedDataSource> ExtractCsvTables();

  // Notify observers of new data
  void NotifyDataSourcesChanged();

  // Handle stream detection from DOMStreamTracker
  void OnStreamDetected(const StreamDefinition& stream);

  // Extractors
  std::unique_ptr<HtmlStructureExtractor> html_extractor_;
  std::unique_ptr<JsonExtractor> json_extractor_;
  std::unique_ptr<JsonLdExtractor> jsonld_extractor_;
  std::unique_ptr<CsvTableExtractor> csv_extractor_;

  // Stream detector
  std::unique_ptr<DOMStreamTracker> stream_tracker_;

  // Cached results
  std::vector<ExtractedDataSource> last_detected_sources_;

  // Live tracking
  bool live_tracking_enabled_ = false;
  DataSourcesCallback data_sources_callback_;

  // Stream tracking
  StreamDetectedCallback stream_detected_callback_;

  base::WeakPtrFactory<PageDataDetector> weak_factory_{this};
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_EXTRACTION_PAGE_DATA_DETECTOR_H_
