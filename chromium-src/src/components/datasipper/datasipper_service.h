// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_DATASIPPER_SERVICE_H_
#define COMPONENTS_DATASIPPER_DATASIPPER_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents_observer.h"

#if defined(COMPONENT_BUILD)
#if defined(DATASIPPER_IMPLEMENTATION)
#define DATASIPPER_EXPORT __attribute__((visibility("default")))
#else
#define DATASIPPER_EXPORT
#endif
#else  // defined(COMPONENT_BUILD)
#define DATASIPPER_EXPORT
#endif

namespace content {
class WebContents;
}  // namespace content

namespace datasipper {

class DataSipperDatabase;
class NetworkStreamAnalyzer;
class StreamRegistry;
class PageDataDetector;
class WorkflowEngine;
class WorkflowStorage;
struct ExtractedDataSource;
struct StreamDefinition;

// Observer for tracking tab lifecycle across all browser windows
class TabLifecycleObserver {
 public:
  virtual void OnTabCreated(content::WebContents* web_contents) = 0;
  virtual void OnTabClosing(content::WebContents* web_contents) = 0;
  virtual ~TabLifecycleObserver() = default;
};

// Main service for DataSipper - coordinates extraction, workflows, and storage
class DATASIPPER_EXPORT DataSipperService : public KeyedService,
                                             public TabLifecycleObserver {
 public:
  explicit DataSipperService(const base::FilePath& database_path);
  ~DataSipperService() override;  // Defined in .cc due to incomplete types

  DataSipperService(const DataSipperService&) = delete;
  DataSipperService& operator=(const DataSipperService&) = delete;

  bool Initialize();
  void Shutdown() override;

  // Tab lifecycle tracking (TabLifecycleObserver)
  void OnTabCreated(content::WebContents* web_contents) override;
  void OnTabClosing(content::WebContents* web_contents) override;

  // Manual tab registration (for existing tabs at startup)
  void RegisterTab(content::WebContents* web_contents);
  void UnregisterTab(content::WebContents* web_contents);

  // Workflow management
  WorkflowEngine* workflow_engine() const { return workflow_engine_.get(); }
  WorkflowStorage* workflow_storage() const { return workflow_storage_.get(); }

  // Accessor for database - returns nullptr if not initialized
  DataSipperDatabase* database() const { return database_.get(); }

  // Stream registry access
  StreamRegistry* stream_registry() const { return stream_registry_.get(); }

  // Called by DataSipperNetworkBridge when network request is captured
  // (for extraction and workflow processing)
  void OnNetworkRequestCaptured(
      side_panel::mojom::NetworkRequestDataPtr request);

 private:
  // Internal tracker for per-tab PageDataDetector instances
  struct TabTracker {
    TabTracker();
    ~TabTracker();

    raw_ptr<content::WebContents> web_contents;
    std::unique_ptr<PageDataDetector> detector;
  };

  bool InitializeDatabase();
  bool InitializeStorageComponents();
  bool InitializeWorkflowSystem();

  // Handle extraction results and trigger workflows
  void OnDataExtracted(content::WebContents* web_contents,
                       const std::vector<ExtractedDataSource>& sources);

  // Handle detected data streams
  void OnStreamDetected(content::WebContents* web_contents,
                        const StreamDefinition& stream);

  // Check and execute workflows matching the current page
  void TriggerMatchingWorkflows(content::WebContents* web_contents,
                                const GURL& url,
                                const std::vector<ExtractedDataSource>& sources);

  base::FilePath database_path_;
  std::unique_ptr<DataSipperDatabase> database_;
  std::unique_ptr<WorkflowEngine> workflow_engine_;
  std::unique_ptr<WorkflowStorage> workflow_storage_;

  // Network stream analyzer (global, not per-tab)
  std::unique_ptr<NetworkStreamAnalyzer> network_stream_analyzer_;

  // Stream registry
  std::unique_ptr<StreamRegistry> stream_registry_;

  // Map of WebContents to their tracking data
  std::map<content::WebContents*, std::unique_ptr<TabTracker>> tab_trackers_;

  bool initialized_ = false;
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_DATASIPPER_SERVICE_H_
