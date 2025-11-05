// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_EXPORTERS_KAFKA_EXPORTER_H_
#define COMPONENTS_DATASIPPER_EXPORTERS_KAFKA_EXPORTER_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/datasipper/workflow/nodes/export_node.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace datasipper {

// Exports data to Kafka via REST Proxy
class KafkaExporter : public ExportNode {
 public:
  explicit KafkaExporter(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~KafkaExporter() override;

  KafkaExporter(const KafkaExporter&) = delete;
  KafkaExporter& operator=(const KafkaExporter&) = delete;

  // ExportNode implementation
  void Export(const base::Value& data,
             const ExportConfig& config,
             ExportCallback callback) override;

  void TestConnection(const ExportConfig& config,
                     base::OnceCallback<void(bool success)> callback) override;

 private:
  void OnExportComplete(ExportCallback callback,
                        std::unique_ptr<std::string> response_body);

  void OnTestComplete(base::OnceCallback<void(bool success)> callback,
                      std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_EXPORTERS_KAFKA_EXPORTER_H_
