// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/workflow/nodes/ai_processor_node.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace datasipper {

// AIConfig implementation
AIProcessorNode::AIConfig::AIConfig() = default;
AIProcessorNode::AIConfig::AIConfig(const AIConfig&) = default;
AIProcessorNode::AIConfig& AIProcessorNode::AIConfig::operator=(const AIConfig&) = default;
AIProcessorNode::AIConfig::~AIConfig() = default;

base::Value AIProcessorNode::AIConfig::ToJson() const {
  base::Value::Dict dict;
  dict.Set("api_url", api_url);
  dict.Set("api_key", api_key);
  dict.Set("model", model);
  dict.Set("system_prompt", system_prompt);
  dict.Set("user_prompt_template", user_prompt_template);
  dict.Set("temperature", temperature);
  dict.Set("max_tokens", max_tokens);
  dict.Set("response_format", response_format);
  return base::Value(std::move(dict));
}

AIProcessorNode::AIConfig AIProcessorNode::AIConfig::FromJson(const base::Value& json) {
  AIConfig config;

  if (!json.is_dict()) {
    return config;
  }

  const base::Value::Dict& dict = json.GetDict();

  if (const std::string* url = dict.FindString("api_url")) {
    config.api_url = *url;
  }
  if (const std::string* key = dict.FindString("api_key")) {
    config.api_key = *key;
  }
  if (const std::string* model = dict.FindString("model")) {
    config.model = *model;
  }
  if (const std::string* sys_prompt = dict.FindString("system_prompt")) {
    config.system_prompt = *sys_prompt;
  }
  if (const std::string* user_template = dict.FindString("user_prompt_template")) {
    config.user_prompt_template = *user_template;
  }
  config.temperature = dict.FindDouble("temperature").value_or(0.7);
  config.max_tokens = dict.FindInt("max_tokens").value_or(1000);
  if (const std::string* format = dict.FindString("response_format")) {
    config.response_format = *format;
  }

  return config;
}

// AIProcessorNode implementation
AIProcessorNode::AIProcessorNode(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

AIProcessorNode::~AIProcessorNode() = default;

void AIProcessorNode::Process(const base::Value& input,
                              const AIConfig& config,
                              AICallback callback) {
  if (config.api_key.empty()) {
    std::move(callback).Run(false, base::Value(), "API key is required");
    return;
  }

  // Build prompt from template
  std::string user_prompt = BuildPrompt(config.user_prompt_template, input);

  // Create OpenAI request
  std::string request_body = CreateOpenAIRequest(user_prompt, config);

  // Create HTTP request
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(config.api_url);
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader("Content-Type", "application/json");
  resource_request->headers.SetHeader("Authorization",
                                     base::StrCat({"Bearer ", config.api_key}));

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("datasipper_ai_processing", R"(
        semantics {
          sender: "DataSipper AI Processor"
          description: "Sends extracted data to AI API for processing"
          trigger: "User-configured workflow execution with AI node"
          data: "Extracted structured data and user-defined prompts"
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "Users configure AI API endpoints in DataSipper workflows"
        })");

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  url_loader_->AttachStringForUpload(request_body, "application/json");

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&AIProcessorNode::OnAIResponse,
                     base::Unretained(this),
                     std::move(callback),
                     config));
}

std::string AIProcessorNode::BuildPrompt(const std::string& template_str,
                                         const base::Value& input) {
  std::string prompt = template_str;

  // Replace {{variable}} placeholders with actual values
  // Support {{field.nested}} syntax
  size_t pos = 0;
  while ((pos = prompt.find("{{", pos)) != std::string::npos) {
    size_t end_pos = prompt.find("}}", pos);
    if (end_pos == std::string::npos) {
      break;
    }

    std::string var_name = prompt.substr(pos + 2, end_pos - pos - 2);
    base::TrimWhitespaceASCII(var_name, base::TRIM_ALL, &var_name);

    // Extract value from input using JSONPath-like syntax
    std::string value_str;
    if (input.is_dict()) {
      const base::Value* value = input.GetDict().Find(var_name);
      if (value) {
        if (value->is_string()) {
          value_str = value->GetString();
        } else {
          base::JSONWriter::Write(*value, &value_str);
        }
      }
    } else if (input.is_string()) {
      value_str = input.GetString();
    } else {
      base::JSONWriter::Write(input, &value_str);
    }

    prompt.replace(pos, end_pos - pos + 2, value_str);
    pos += value_str.length();
  }

  return prompt;
}

std::string AIProcessorNode::CreateOpenAIRequest(const std::string& prompt,
                                                 const AIConfig& config) {
  base::Value::Dict request;
  request.Set("model", config.model);

  base::Value::List messages;

  // Add system prompt if provided
  if (!config.system_prompt.empty()) {
    base::Value::Dict system_msg;
    system_msg.Set("role", "system");
    system_msg.Set("content", config.system_prompt);
    messages.Append(std::move(system_msg));
  }

  // Add user prompt
  base::Value::Dict user_msg;
  user_msg.Set("role", "user");
  user_msg.Set("content", prompt);
  messages.Append(std::move(user_msg));

  request.Set("messages", std::move(messages));
  request.Set("temperature", config.temperature);
  request.Set("max_tokens", config.max_tokens);

  // Add response format for JSON mode
  if (config.response_format == "json") {
    base::Value::Dict response_format;
    response_format.Set("type", "json_object");
    request.Set("response_format", std::move(response_format));
  }

  std::string json_str;
  base::JSONWriter::Write(base::Value(std::move(request)), &json_str);
  return json_str;
}

void AIProcessorNode::OnAIResponse(AICallback callback,
                                   const AIConfig& config,
                                   std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  if (response_code != 200 || !response_body) {
    std::string error = base::StrCat({
        "AI API request failed with HTTP ", base::NumberToString(response_code)});
    LOG(ERROR) << error;
    std::move(callback).Run(false, base::Value(), error);
    return;
  }

  // Parse response
  base::Value result = ParseOpenAIResponse(*response_body, config);

  if (result.is_none()) {
    std::move(callback).Run(false, base::Value(), "Failed to parse AI response");
    return;
  }

  LOG(INFO) << "AI processing completed successfully";
  std::move(callback).Run(true, std::move(result), "");
}

base::Value AIProcessorNode::ParseOpenAIResponse(const std::string& response_body,
                                                 const AIConfig& config) {
  auto parsed = base::JSONReader::Read(response_body);
  if (!parsed || !parsed->is_dict()) {
    LOG(ERROR) << "Invalid JSON response from AI API";
    return base::Value();
  }

  const base::Value::Dict& response = parsed->GetDict();

  // Extract choices array
  const base::Value::List* choices = response.FindList("choices");
  if (!choices || choices->empty()) {
    LOG(ERROR) << "No choices in AI response";
    return base::Value();
  }

  // Get first choice
  const base::Value& first_choice = (*choices)[0];
  if (!first_choice.is_dict()) {
    return base::Value();
  }

  // Extract message content
  const base::Value::Dict* message = first_choice.GetDict().FindDict("message");
  if (!message) {
    return base::Value();
  }

  const std::string* content = message->FindString("content");
  if (!content) {
    return base::Value();
  }

  // If response format is JSON, parse the content
  if (config.response_format == "json") {
    auto json_result = base::JSONReader::Read(*content);
    if (json_result) {
      return std::move(*json_result);
    }
  }

  // Return as text
  return base::Value(*content);
}

}  // namespace datasipper
