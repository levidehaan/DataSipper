// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/exporters/postgresql_exporter.h"

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

PostgreSQLExporter::PostgreSQLExporter(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

PostgreSQLExporter::~PostgreSQLExporter() = default;

void PostgreSQLExporter::Export(const base::Value& data,
                                const ExportConfig& config,
                                ExportCallback callback) {
  // Get PostgREST or custom PostgreSQL REST API URL
  const std::string* api_url = config.settings.FindString("api_url");
  const std::string* table = config.settings.FindString("table");

  if (!api_url || !table) {
    LOG(ERROR) << "PostgreSQL export requires 'api_url' and 'table' settings";
    std::move(callback).Run(false, "Missing required settings");
    return;
  }

  // Convert data to JSON
  std::string json_body;
  base::JSONWriter::Write(data, &json_body);

  // Build request URL
  std::string url = base::StrCat({*api_url, "/", *table});

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader("Content-Type", "application/json");
  resource_request->headers.SetHeader("Prefer", "return=minimal");  // PostgREST

  // Add auth header if provided
  const std::string* auth_token = config.settings.FindString("auth_token");
  if (auth_token) {
    resource_request->headers.SetHeader("Authorization",
                                       base::StrCat({"Bearer ", *auth_token}));
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("datasipper_postgresql_export", R"(
        semantics {
          sender: "DataSipper PostgreSQL Exporter"
          description: "Exports extracted web data to PostgreSQL"
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

  url_loader_->AttachStringForUpload(json_body, "application/json");

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PostgreSQLExporter::OnExportComplete,
                     base::Unretained(this),
                     std::move(callback)));
}

void PostgreSQLExporter::TestConnection(
    const ExportConfig& config,
    base::OnceCallback<void(bool success)> callback) {
  const std::string* api_url = config.settings.FindString("api_url");

  if (!api_url) {
    std::move(callback).Run(false);
    return;
  }

  // Test with simple GET to root endpoint
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(*api_url);
  resource_request->method = "GET";

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("datasipper_postgresql_test", R"(
        semantics {
          sender: "DataSipper PostgreSQL Exporter"
          description: "Tests connection to PostgreSQL REST API"
          trigger: "User tests PostgreSQL export configuration"
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
      base::BindOnce(&PostgreSQLExporter::OnTestComplete,
                     base::Unretained(this),
                     std::move(callback)));
}

void PostgreSQLExporter::OnExportComplete(
    ExportCallback callback,
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  bool success = (response_code >= 200 && response_code < 300);

  if (success) {
    LOG(INFO) << "Successfully exported to PostgreSQL";
    std::move(callback).Run(true, "");
  } else {
    std::string error = base::StrCat({
        "PostgreSQL export failed with HTTP ", base::NumberToString(response_code)});
    LOG(ERROR) << error;
    std::move(callback).Run(false, error);
  }
}

void PostgreSQLExporter::OnTestComplete(
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
