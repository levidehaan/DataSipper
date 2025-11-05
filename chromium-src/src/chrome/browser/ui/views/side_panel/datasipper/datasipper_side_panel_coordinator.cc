// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/datasipper/datasipper_side_panel_coordinator.h"

#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

using SidePanelWebUIViewT_DataSipperUI =
    SidePanelWebUIViewT<DataSipperUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_DataSipperUI,
                        SidePanelWebUIViewT)
END_METADATA

DataSipperSidePanelCoordinator::DataSipperSidePanelCoordinator() = default;

void DataSipperSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  LOG(INFO) << "DataSipper: CreateAndRegisterEntry called";
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kDataSipper,
      base::BindRepeating(
          &DataSipperSidePanelCoordinator::CreateDataSipperWebView,
          base::Unretained(this))));
  LOG(INFO) << "DataSipper: Entry registered successfully";
}

std::unique_ptr<views::View>
DataSipperSidePanelCoordinator::CreateDataSipperWebView(
    SidePanelEntryScope& scope) {
  LOG(INFO) << "DataSipper: CreateDataSipperWebView called";
  auto webui_wrapper = std::make_unique<WebUIContentsWrapperT<DataSipperUI>>(
      GURL(chrome::kChromeUIDataSipperSidePanelURL),
      scope.GetBrowserWindowInterface().GetProfile(),
      /*title_string_id=*/0,  // No title string needed for now
      /*esc_closes_ui=*/false);

  auto datasipper_web_view =
      std::make_unique<SidePanelWebUIViewT<DataSipperUI>>(
          scope, base::RepeatingClosure(), base::RepeatingClosure(),
          std::move(webui_wrapper));

  // Mark WebUI as available immediately to avoid deadlock
  // (JavaScript can't call ShowUI() until the panel is shown, but the panel
  //  won't show until ShowUI() is called - chicken and egg problem)
  SidePanelUtil::GetSidePanelContentProxy(datasipper_web_view.get())
      ->SetAvailable(true);
  LOG(INFO) << "DataSipper: WebUI content marked as available";

  LOG(INFO) << "DataSipper: WebView created successfully";
  return datasipper_web_view;
}
