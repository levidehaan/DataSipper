// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_STREAMING_DOM_STREAM_TRACKER_H_
#define COMPONENTS_DATASIPPER_STREAMING_DOM_STREAM_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/datasipper/streaming/stream_types.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace datasipper {

// Tracks DOM mutations and detects data streams in web pages
// by injecting JavaScript and analyzing reported patterns
class DOMStreamTracker : public content::WebContentsObserver {
 public:
  using StreamDetectedCallback =
      base::RepeatingCallback<void(const StreamDefinition&)>;

  explicit DOMStreamTracker(content::WebContents* web_contents);
  ~DOMStreamTracker() override;

  DOMStreamTracker(const DOMStreamTracker&) = delete;
  DOMStreamTracker& operator=(const DOMStreamTracker&) = delete;

  // Enable/disable tracking
  void EnableTracking(StreamDetectedCallback callback);
  void DisableTracking();
  bool IsTracking() const { return tracking_enabled_; }

  // Manually trigger page scan
  void ScanPage();

  // Get currently tracked streams
  std::vector<StreamDefinition> GetActiveStreams() const;

  // content::WebContentsObserver implementation
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;

 private:
  // Inject tracker JavaScript into the page
  void InjectTrackerScript(content::RenderFrameHost* render_frame_host);

  // Start polling for messages from JavaScript
  void StartMessagePolling();
  void StopMessagePolling();
  void PollMessages();
  void OnMessagesPoll(base::Value result);

  // Handle JavaScript messages
  void OnTrackerMessage(const base::Value& message);

  // Process detected stream from JavaScript
  void ProcessDetectedStream(const base::Value::Dict& stream_data);

  // Convert JavaScript data type to DataType enum
  DataType ParseDataType(const std::string& type_str);

  // Classify pattern type based on statistics
  PatternType ClassifyPatternType(double avg_interval_ms,
                                   double coefficient_of_variation,
                                   bool is_regular);

  // Callback for stream detection
  StreamDetectedCallback stream_detected_callback_;

  // Tracking state
  bool tracking_enabled_ = false;
  bool script_injected_ = false;

  // Message polling
  base::RepeatingTimer message_poll_timer_;

  // Active streams (keyed by CSS selector)
  std::map<std::string, StreamDefinition> active_streams_;

  base::WeakPtrFactory<DOMStreamTracker> weak_factory_{this};
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_STREAMING_DOM_STREAM_TRACKER_H_
