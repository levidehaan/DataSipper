// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_WORKFLOW_NODES_AI_NODE_H_
#define COMPONENTS_DATASIPPER_WORKFLOW_NODES_AI_NODE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace datasipper {

// Configuration for AI processing node
struct AINodeConfig {
  AINodeConfig();
  AINodeConfig(const AINodeConfig&);
  AINodeConfig& operator=(const AINodeConfig&);
  ~AINodeConfig();

  // API endpoint (OpenAI-compatible)
  std::string endpoint_url;  // e.g., "https://api.openai.com/v1/chat/completions"
  std::string api_key;
  std::string model;  // e.g., "gpt-4", "gpt-3.5-turbo", "local-llama"

  // Prompts
  std::string system_prompt;
  std::string user_prompt_template;  // Supports {{variable}} substitution

  // Structured output (JSON schema for tool/function calling)
  base::Value tool_schema;  // JSON schema for forced structured output
  bool force_structured_output = true;

  // Generation parameters
  int max_tokens = 1000;
  float temperature = 0.7;
  float top_p = 1.0;
  int timeout_seconds = 30;

  // Retry logic
  int max_retries = 3;
  int retry_delay_ms = 1000;

  // Parse from workflow node config
  static AINodeConfig FromJson(const base::Value& config);
  base::Value ToJson() const;
};

// AI processor node - sends data to LLM and processes response
class AINode {
 public:
  using CompletionCallback = base::OnceCallback<void(base::Value result)>;
  using ErrorCallback = base::OnceCallback<void(const std::string& error)>;

  explicit AINode(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AINode();

  AINode(const AINode&) = delete;
  AINode& operator=(const AINode&) = delete;

  // Execute AI processing
  void Execute(const AINodeConfig& config,
               const base::Value& input_data,
               CompletionCallback completion_callback,
               ErrorCallback error_callback);

  // Cancel ongoing request
  void Cancel();

 private:
  // Build request payload
  base::Value BuildRequestPayload(const AINodeConfig& config,
                                  const base::Value& input_data);

  // Substitute variables in prompt template
  std::string SubstitutePromptVariables(const std::string& template_str,
                                       const base::Value& data);

  // Build OpenAI-compatible chat completion request
  base::Value BuildChatCompletionRequest(const AINodeConfig& config,
                                         const std::string& user_message);

  // Build function/tool calling request for structured output
  base::Value BuildToolCallingRequest(const AINodeConfig& config,
                                     const std::string& user_message);

  // Send HTTP request to AI endpoint
  void SendRequest(const std::string& url,
                  const std::string& api_key,
                  const base::Value& payload);

  // Handle API response
  void OnRequestComplete(std::unique_ptr<std::string> response_body);

  // Parse response and extract result
  base::Value ParseResponse(const std::string& response_body,
                           const AINodeConfig& config);

  // Extract structured data from tool call response
  base::Value ExtractToolCallResult(const base::Value& response);

  // Retry logic
  void RetryRequest();

  // Current execution state
  AINodeConfig current_config_;
  base::Value current_input_;
  CompletionCallback completion_callback_;
  ErrorCallback error_callback_;

  int retry_count_ = 0;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<AINode> weak_factory_{this};
};

// AI test harness for validating AI node configurations
class AITestHarness {
 public:
  struct TestCase {
    TestCase();
    ~TestCase();

    std::string name;
    base::Value input_data;
    base::Value expected_output;
    std::string expected_schema_match;  // JSONPath to validate
  };

  struct TestResult {
    TestResult();
    ~TestResult();

    std::string test_name;
    bool passed = false;
    base::Value actual_output;
    base::Value expected_output;
    std::string error_message;
    int64_t execution_time_ms = 0;
  };

  using TestCallback =
      base::OnceCallback<void(std::vector<TestResult> results)>;

  explicit AITestHarness(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AITestHarness();

  // Run test cases
  void RunTests(const AINodeConfig& config,
                const std::vector<TestCase>& test_cases,
                TestCallback callback);

 private:
  void RunNextTest();
  void OnTestComplete(base::Value result);
  void OnTestError(const std::string& error);

  AINodeConfig config_;
  std::vector<TestCase> test_cases_;
  std::vector<TestResult> results_;
  size_t current_test_index_ = 0;
  TestCallback callback_;

  std::unique_ptr<AINode> ai_node_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_WORKFLOW_NODES_AI_NODE_H_
