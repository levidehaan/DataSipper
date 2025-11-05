// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_DATASIPPER_DATASIPPER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_DATASIPPER_DATASIPPER_UI_H_

#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace datasipper {
class DataSipperService;
}

class DataSipperUI;

class DataSipperUIConfig : public DefaultTopChromeWebUIConfig<DataSipperUI> {
 public:
  DataSipperUIConfig();
};

class DataSipperUI : public TopChromeWebUIController,
                     public side_panel::mojom::DataSipperPageHandlerFactory {
 public:
  explicit DataSipperUI(content::WebUI* web_ui);
  ~DataSipperUI() override;

  DataSipperUI(const DataSipperUI&) = delete;
  DataSipperUI& operator=(const DataSipperUI&) = delete;

  // Bind the factory interface
  void BindInterface(
      mojo::PendingReceiver<side_panel::mojom::DataSipperPageHandlerFactory>
          receiver);

  // side_panel::mojom::DataSipperPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<side_panel::mojom::DataSipperPageHandler> receiver)
      override;

  static constexpr std::string GetWebUIName() { return "DataSipperSidePanel"; }

 private:
  std::unique_ptr<side_panel::mojom::DataSipperPageHandler> page_handler_;
  mojo::Receiver<side_panel::mojom::DataSipperPageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_DATASIPPER_DATASIPPER_UI_H_
