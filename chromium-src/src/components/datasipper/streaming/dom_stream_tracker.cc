// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/streaming/dom_stream_tracker.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace datasipper {

namespace {

// Load the JavaScript tracker code
// TODO: Convert to GRIT resource after registering IDs in tools/gritsettings/resource_ids.spec
std::string LoadTrackerScript() {
  // For now, we embed the full tracker directly
  // In production, this would be loaded from a GRIT resource
  // The full implementation is in components/datasipper/resources/dom_mutation_tracker.js

  // Return the full JavaScript implementation (embedded for now)
  // This is the same content as dom_mutation_tracker.js
#include "components/datasipper/resources/dom_mutation_tracker_inline.inc"

  return kDomMutationTrackerScript;
}

}  // namespace

DOMStreamTracker::DOMStreamTracker(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(web_contents);
  LOG(INFO) << "DOMStreamTracker created for: "
            << web_contents->GetLastCommittedURL().spec();
}

DOMStreamTracker::~DOMStreamTracker() {
  DisableTracking();
}

void DOMStreamTracker::EnableTracking(StreamDetectedCallback callback) {
  tracking_enabled_ = true;
  stream_detected_callback_ = std::move(callback);

  LOG(INFO) << "DOM stream tracking enabled";

  // Inject into current frame if available
  if (web_contents()) {
    content::RenderFrameHost* main_frame =
        web_contents()->GetPrimaryMainFrame();
    if (main_frame) {
      InjectTrackerScript(main_frame);
    }
  }

  // Start polling for messages from JavaScript
  StartMessagePolling();
}

void DOMStreamTracker::DisableTracking() {
  tracking_enabled_ = false;
  stream_detected_callback_.Reset();
  script_injected_ = false;
  active_streams_.clear();

  // Stop message polling
  StopMessagePolling();

  LOG(INFO) << "DOM stream tracking disabled";
}

void DOMStreamTracker::ScanPage() {
  if (!tracking_enabled_ || !web_contents()) {
    return;
  }

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  if (!main_frame) {
    return;
  }

  // Call the scan function in JavaScript
  const char16_t kScanScript[] = uR"(
    (function() {
      if (window.__dataSipperStreamTracker) {
        return window.__dataSipperStreamTracker.scanPage();
      }
      return 0;
    })();
  )";

  main_frame->ExecuteJavaScriptForTests(
      kScanScript,
      base::BindOnce([](base::Value result) {
        if (result.is_int()) {
          DVLOG(1) << "DOM scan found " << result.GetInt() << " elements";
        }
      }),
      content::ISOLATED_WORLD_ID_GLOBAL);
}

std::vector<StreamDefinition> DOMStreamTracker::GetActiveStreams() const {
  std::vector<StreamDefinition> streams;
  streams.reserve(active_streams_.size());

  for (const auto& [selector, stream] : active_streams_) {
    streams.push_back(stream);
  }

  return streams;
}

void DOMStreamTracker::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  if (tracking_enabled_) {
    // Reset state for new page
    active_streams_.clear();
    script_injected_ = false;

    InjectTrackerScript(render_frame_host);
  }
}

void DOMStreamTracker::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  if (tracking_enabled_ && !script_injected_) {
    InjectTrackerScript(render_frame_host);
  }
}

void DOMStreamTracker::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  // Inject on frame creation if tracking is enabled
  if (tracking_enabled_) {
    InjectTrackerScript(render_frame_host);
  }
}

void DOMStreamTracker::InjectTrackerScript(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host || script_injected_) {
    return;
  }

  std::string script = LoadTrackerScript();
  if (script.empty()) {
    LOG(ERROR) << "Failed to load tracker script";
    return;
  }

  DVLOG(1) << "Injecting DOM tracker script into: "
           << render_frame_host->GetLastCommittedURL().spec();

  // Convert to UTF-16 for ExecuteJavaScript
  std::u16string script_utf16 = base::UTF8ToUTF16(script);

  // Inject the script
  render_frame_host->ExecuteJavaScriptForTests(
      script_utf16,
      base::BindOnce([](base::Value result) {
        DVLOG(1) << "DOM tracker script injected successfully";
      }),
      content::ISOLATED_WORLD_ID_GLOBAL);

  script_injected_ = true;

  // Set up message listener (using WebContents console message observer)
  // Note: In production, we'd use a dedicated message channel
  // For now, the JavaScript will use window.postMessage which we can intercept
  // via WebContentsObserver::DidReceiveMessage when that's implemented
}

void DOMStreamTracker::OnTrackerMessage(const base::Value& message) {
  if (!message.is_dict()) {
    return;
  }

  const base::Value::Dict& dict = message.GetDict();

  const std::string* type = dict.FindString("type");
  if (!type) {
    return;
  }

  if (*type == "DATASIPPER_TRACKER_READY") {
    DVLOG(1) << "DOM tracker ready in page";
    return;
  }

  if (*type == "DATASIPPER_STREAM_DETECTED") {
    const base::Value::Dict* stream_data = dict.FindDict("stream");
    if (stream_data) {
      ProcessDetectedStream(*stream_data);
    }
  }
}

void DOMStreamTracker::ProcessDetectedStream(
    const base::Value::Dict& stream_data) {

  const std::string* selector = stream_data.FindString("selector");
  if (!selector || selector->empty()) {
    return;
  }

  // Check if we've already reported this stream
  if (active_streams_.find(*selector) != active_streams_.end()) {
    // Update existing stream
    StreamDefinition& stream = active_streams_[*selector];
    stream.last_seen = base::Time::Now();

    std::optional<int> update_count = stream_data.FindInt("updateCount");
    if (update_count) {
      stream.event_count = *update_count;
    }

    const std::string* current_value = stream_data.FindString("currentValue");
    if (current_value) {
      // Could store latest value in metadata if needed
    }

    return;
  }

  // Create new stream definition
  StreamDefinition stream;
  stream.stream_id = "dom_" + *selector;  // Generate unique ID
  stream.label = *selector;  // Use selector as label
  stream.type = StreamType::DOM_MUTATION;
  stream.source_pattern = *selector;
  stream.url = web_contents() ? web_contents()->GetLastCommittedURL().spec() : "";
  stream.first_seen = base::Time::Now();
  stream.last_seen = stream.first_seen;
  stream.is_active = true;

  // Parse timing data
  std::optional<double> avg_interval = stream_data.FindDouble("avgIntervalMs");
  if (avg_interval) {
    stream.timing.avg_interval_ms = *avg_interval;
  }

  std::optional<double> std_dev = stream_data.FindDouble("stdDeviation");
  if (std_dev) {
    stream.timing.std_deviation = *std_dev;
  }

  std::optional<bool> is_regular = stream_data.FindBool("isRegular");
  if (is_regular) {
    stream.timing.is_regular = *is_regular;
  }

  std::optional<double> cv = stream_data.FindDouble("coefficientOfVariation");
  double coefficient_of_variation = cv.value_or(0.0);

  // Classify pattern type
  stream.timing.type = ClassifyPatternType(
      stream.timing.avg_interval_ms,
      coefficient_of_variation,
      stream.timing.is_regular);

  // Parse data type
  const std::string* data_type_str = stream_data.FindString("dataType");
  if (data_type_str) {
    stream.data_type = ParseDataType(*data_type_str);
  }

  // Parse update count
  std::optional<int> update_count = stream_data.FindInt("updateCount");
  if (update_count) {
    stream.event_count = *update_count;
  }

  LOG(INFO) << "DOM stream detected: " << stream.label
            << " (type: " << static_cast<int>(stream.data_type)
            << ", interval: " << stream.timing.avg_interval_ms << "ms"
            << ", updates: " << stream.event_count << ")";

  // Store stream
  active_streams_[*selector] = stream;

  // Notify callback
  if (stream_detected_callback_) {
    stream_detected_callback_.Run(stream);
  }
}

DataType DOMStreamTracker::ParseDataType(const std::string& type_str) {
  if (type_str == "NUMERIC") {
    return DataType::NUMERIC;
  } else if (type_str == "JSON") {
    return DataType::JSON;
  } else if (type_str == "TEXT") {
    return DataType::TEXT;
  } else if (type_str == "XML") {
    return DataType::XML;
  } else if (type_str == "BINARY") {
    return DataType::BINARY;
  }

  return DataType::UNKNOWN;
}

PatternType DOMStreamTracker::ClassifyPatternType(
    double avg_interval_ms,
    double coefficient_of_variation,
    bool is_regular) {

  if (!is_regular) {
    // High variance - likely event-driven
    if (coefficient_of_variation > 0.5) {
      return PatternType::EVENT_DRIVEN;
    }
    // Medium variance - could be burst pattern
    return PatternType::BURST;
  }

  // Regular pattern - classify as polling
  // Could further classify based on interval:
  // - Very fast (< 1s): Real-time
  // - Medium (1-60s): Polling
  // - Slow (> 60s): Periodic updates

  return PatternType::POLLING;
}

void DOMStreamTracker::StartMessagePolling() {
  if (message_poll_timer_.IsRunning()) {
    return;
  }

  DVLOG(1) << "Starting message polling";

  // Poll for messages every 2 seconds
  message_poll_timer_.Start(FROM_HERE, base::Seconds(2), this,
                             &DOMStreamTracker::PollMessages);
}

void DOMStreamTracker::StopMessagePolling() {
  if (message_poll_timer_.IsRunning()) {
    message_poll_timer_.Stop();
    DVLOG(1) << "Stopped message polling";
  }
}

void DOMStreamTracker::PollMessages() {
  if (!web_contents() || !tracking_enabled_) {
    return;
  }

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  if (!main_frame) {
    return;
  }

  // Call JavaScript to get pending messages
  const char16_t kGetMessagesScript[] = uR"(
    (function() {
      if (window.__dataSipperStreamTracker && window.__dataSipperStreamTracker.getMessages) {
        return window.__dataSipperStreamTracker.getMessages();
      }
      return [];
    })();
  )";

  main_frame->ExecuteJavaScriptForTests(
      kGetMessagesScript,
      base::BindOnce(&DOMStreamTracker::OnMessagesPoll,
                     weak_factory_.GetWeakPtr()),
      content::ISOLATED_WORLD_ID_GLOBAL);
}

void DOMStreamTracker::OnMessagesPoll(base::Value result) {
  if (!result.is_list()) {
    return;
  }

  const base::Value::List& messages = result.GetList();

  for (const base::Value& message : messages) {
    if (message.is_dict()) {
      OnTrackerMessage(message);
    }
  }
}

}  // namespace datasipper
