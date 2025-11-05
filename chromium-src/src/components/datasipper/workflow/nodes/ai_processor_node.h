// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_WORKFLOW_NODES_AI_PROCESSOR_NODE_H_
#define COMPONENTS_DATASIPPER_WORKFLOW_NODES_AI_PROCESSOR_NODE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace datasipper {

// AI processing using OpenAI-compatible APIs
class AIProcessorNode {
 public:
  using AICallback = base::OnceCallback<void(bool success,
                                              base::Value result,
                                              std::string error)>;

  // AI configuration
  struct AIConfig {
    AIConfig();
    AIConfig(const AIConfig&);
    AIConfig& operator=(const AIConfig&);
    ~AIConfig();

    std::string api_url = "https://api.openai.com/v1/chat/completions";
    std::string api_key;
    std::string model = "gpt-3.5-turbo";
    std::string system_prompt;
    std::string user_prompt_template;  // With {{variable}} placeholders
    double temperature = 0.7;
    int max_tokens = 1000;
    std::string response_format = "text";  // "text" or "json"

    base::Value ToJson() const;
    static AIConfig FromJson(const base::Value& json);
  };

  explicit AIProcessorNode(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AIProcessorNode();

  AIProcessorNode(const AIProcessorNode&) = delete;
  AIProcessorNode& operator=(const AIProcessorNode&) = delete;

  // Process data with AI
  void Process(const base::Value& input,
               const AIConfig& config,
               AICallback callback);

 private:
  // Build prompt from template and input data
  std::string BuildPrompt(const std::string& template_str,
                          const base::Value& input);

  // Create OpenAI API request
  std::string CreateOpenAIRequest(const std::string& prompt,
                                  const AIConfig& config);

  // Handle API response
  void OnAIResponse(AICallback callback,
                    const AIConfig& config,
                    std::unique_ptr<std::string> response_body);

  // Parse OpenAI response
  base::Value ParseOpenAIResponse(const std::string& response_body,
                                  const AIConfig& config);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_WORKFLOW_NODES_AI_PROCESSOR_NODE_H_
