// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"

namespace {
constexpr std::underlying_type_t<SidePanelOpenTrigger>
    kInvalidSidePanelOpenTrigger = -1;
}

DEFINE_UI_CLASS_PROPERTY_TYPE(SidePanelOpenTrigger)
DEFINE_UI_CLASS_PROPERTY_KEY(std::underlying_type_t<SidePanelOpenTrigger>,
                             kSidePanelOpenTriggerKey,
                             kInvalidSidePanelOpenTrigger)

actions::ActionItem::InvokeActionCallback CreateToggleSidePanelActionCallback(
    SidePanelEntryKey key,
    Browser* browser) {
  return base::BindRepeating(
      [](SidePanelEntryKey key, Browser* browser, actions::ActionItem* item,
         actions::ActionInvocationContext context) {
        LOG(INFO) << "SidePanel: Toggle callback invoked for entry: " << key.ToString();
        const SidePanelOpenTrigger open_trigger =
            static_cast<SidePanelOpenTrigger>(
                context.GetProperty(kSidePanelOpenTriggerKey));
        CHECK_GE(open_trigger, SidePanelOpenTrigger::kMinValue);
        CHECK_LE(open_trigger, SidePanelOpenTrigger::kMaxValue);
        LOG(INFO) << "SidePanel: Calling Toggle() with trigger: " << static_cast<int>(open_trigger);
        browser->GetFeatures().side_panel_ui()->Toggle(key, open_trigger);
        LOG(INFO) << "SidePanel: Toggle() completed";
      },
      key, browser);
}
