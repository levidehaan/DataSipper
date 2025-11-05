// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/datasipper_resources.h"
#include "chrome/grit/datasipper_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

DataSipperUIConfig::DataSipperUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIDataSipperSidePanelHost) {}

DataSipperUI::DataSipperUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui, true) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // Setup WebUI data source
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIDataSipperSidePanelHost);

  // Setup generated resources
  webui::SetupWebUIDataSource(source, kDatasipperResources,
                               IDR_DATASIPPER_DATASIPPER_HTML);
}

DataSipperUI::~DataSipperUI() = default;

void DataSipperUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::DataSipperPageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void DataSipperUI::CreatePageHandler(
    mojo::PendingReceiver<side_panel::mojom::DataSipperPageHandler> receiver) {
  page_handler_ = std::make_unique<DataSipperPageHandler>(
      std::move(receiver), this, web_ui()->GetWebContents());
}

WEB_UI_CONTROLLER_TYPE_IMPL(DataSipperUI)
