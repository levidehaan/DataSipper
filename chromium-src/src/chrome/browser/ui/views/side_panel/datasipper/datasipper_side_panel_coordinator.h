// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_DATASIPPER_DATASIPPER_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_DATASIPPER_DATASIPPER_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"

namespace views {
class View;
}  // namespace views

class SidePanelEntry;
class SidePanelEntryScope;
class SidePanelRegistry;

// DataSipperSidePanelCoordinator handles the creation and registration of the
// DataSipper side panel entry.
class DataSipperSidePanelCoordinator {
 public:
  DataSipperSidePanelCoordinator();
  DataSipperSidePanelCoordinator(const DataSipperSidePanelCoordinator&) = delete;
  DataSipperSidePanelCoordinator& operator=(
      const DataSipperSidePanelCoordinator&) = delete;
  ~DataSipperSidePanelCoordinator() = default;

  // Creates and registers the DataSipper entry with the global registry.
  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  // Creates the DataSipper WebView.
  std::unique_ptr<views::View> CreateDataSipperWebView(
      SidePanelEntryScope& scope);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_DATASIPPER_DATASIPPER_SIDE_PANEL_COORDINATOR_H_
