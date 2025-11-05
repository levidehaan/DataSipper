// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/exporters/redis_exporter.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace datasipper {

RedisExporter::RedisExporter(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

RedisExporter::~RedisExporter() = default;

void RedisExporter::Export(const base::Value& data,
                           const ExportConfig& config,
                           ExportCallback callback) {
  // Get Redis HTTP API URL (e.g., webdis)
  const std::string* redis_url = config.settings.FindString("redis_url");
  const std::string* key = config.settings.FindString("key");
  const std::string* command = config.settings.FindString("command");  // SET, LPUSH, etc.

  if (!redis_url || !key) {
    LOG(ERROR) << "Redis export requires 'redis_url' and 'key' settings";
    std::move(callback).Run(false, "Missing required settings");
    return;
  }

  std::string cmd = command ? *command : "SET";

  // Serialize data to JSON
  std::string json_data;
  base::JSONWriter::Write(data, &json_data);

  // Build Redis command URL (webdis format: /COMMAND/key/value)
  std::string url = base::StrCat({*redis_url, "/", cmd, "/", *key, "/", json_data});

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);
  resource_request->method = "GET";
  resource_request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("datasipper_redis_export", R"(
        semantics {
          sender: "DataSipper Redis Exporter"
          description: "Exports extracted web data to Redis"
          trigger: "User-configured workflow execution"
          data: "Extracted structured data from web pages"
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "Users can configure this via DataSipper workflows"
        })");

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RedisExporter::OnExportComplete,
                     base::Unretained(this),
                     std::move(callback)));
}

void RedisExporter::TestConnection(
    const ExportConfig& config,
    base::OnceCallback<void(bool success)> callback) {
  const std::string* redis_url = config.settings.FindString("redis_url");

  if (!redis_url) {
    std::move(callback).Run(false);
    return;
  }

  // Test with PING command
  std::string url = base::StrCat({*redis_url, "/PING"});

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);
  resource_request->method = "GET";

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("datasipper_redis_test", R"(
        semantics {
          sender: "DataSipper Redis Exporter"
          description: "Tests connection to Redis"
          trigger: "User tests Redis export configuration"
          data: "No user data, just connection test"
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "Users can configure this via DataSipper workflows"
        })");

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RedisExporter::OnTestComplete,
                     base::Unretained(this),
                     std::move(callback)));
}

void RedisExporter::OnExportComplete(
    ExportCallback callback,
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  bool success = (response_code >= 200 && response_code < 300);

  if (success) {
    LOG(INFO) << "Successfully exported to Redis";
    std::move(callback).Run(true, "");
  } else {
    std::string error = base::StrCat({
        "Redis export failed with HTTP ", base::NumberToString(response_code)});
    LOG(ERROR) << error;
    std::move(callback).Run(false, error);
  }
}

void RedisExporter::OnTestComplete(
    base::OnceCallback<void(bool success)> callback,
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  bool success = (response_code >= 200 && response_code < 300);
  std::move(callback).Run(success);
}

}  // namespace datasipper
