// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/workflow/workflow_engine.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/uuid.h"
#include "components/datasipper/exporters/kafka_exporter.h"
#include "components/datasipper/exporters/postgresql_exporter.h"
#include "components/datasipper/exporters/redis_exporter.h"
#include "components/datasipper/extraction/page_data_detector.h"
#include "components/datasipper/workflow/nodes/ai_processor_node.h"
#include "components/datasipper/workflow/nodes/export_node.h"
#include "components/datasipper/workflow/nodes/filter_node.h"
#include "components/datasipper/workflow/nodes/transform_node.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace datasipper {

// ExecutionState implementation
WorkflowEngine::ExecutionState::ExecutionState() = default;
WorkflowEngine::ExecutionState::~ExecutionState() = default;

WorkflowEngine::WorkflowEngine(
    WorkflowStorage* storage,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : storage_(storage), url_loader_factory_(url_loader_factory) {}

WorkflowEngine::~WorkflowEngine() = default;

void WorkflowEngine::ExecuteWorkflow(const Workflow& workflow,
                                     content::WebContents* web_contents,
                                     ExecutionCallback callback,
                                     const std::vector<base::Value>& data_sources) {
  if (!workflow.IsValid()) {
    LOG(ERROR) << "Cannot execute invalid workflow: " << workflow.id;
    WorkflowExecutionContext context;
    context.status = WorkflowExecutionContext::Status::FAILED;
    context.error_message = "Workflow validation failed";
    std::move(callback).Run(context);
    return;
  }

  // Create execution state
  auto state = std::make_unique<ExecutionState>();
  state->workflow = workflow;
  state->web_contents = web_contents;
  state->callback = std::move(callback);

  // Store extracted data sources for DATA_SOURCE nodes
  for (const auto& source : data_sources) {
    state->data_sources.push_back(source.Clone());
  }

  // Initialize execution context
  state->context.execution_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  state->context.workflow_id = workflow.id;
  state->context.started_at = base::Time::Now();
  state->context.status = WorkflowExecutionContext::Status::PENDING;

  // Build execution plan
  state->execution_order = BuildExecutionPlan(workflow);

  if (state->execution_order.empty()) {
    LOG(ERROR) << "Failed to build execution plan for workflow: " << workflow.id;
    state->context.status = WorkflowExecutionContext::Status::FAILED;
    state->context.error_message = "Failed to build execution plan";
    std::move(state->callback).Run(state->context);
    return;
  }

  std::string execution_id = state->context.execution_id;
  executions_[execution_id] = std::move(state);

  // Start execution
  ExecuteNextNode(execution_id);
}

void WorkflowEngine::CancelExecution(const std::string& execution_id) {
  auto it = executions_.find(execution_id);
  if (it == executions_.end()) {
    return;
  }

  it->second->context.status = WorkflowExecutionContext::Status::CANCELLED;
  it->second->context.completed_at = base::Time::Now();

  std::move(it->second->callback).Run(it->second->context);
  executions_.erase(it);
}

WorkflowExecutionContext::Status WorkflowEngine::GetExecutionStatus(
    const std::string& execution_id) const {
  auto it = executions_.find(execution_id);
  if (it == executions_.end()) {
    return WorkflowExecutionContext::Status::FAILED;
  }
  return it->second->context.status;
}

WorkflowExecutionContext WorkflowEngine::GetExecutionContext(
    const std::string& execution_id) const {
  auto it = executions_.find(execution_id);
  if (it == executions_.end()) {
    return WorkflowExecutionContext();
  }
  return it->second->context;
}

std::vector<std::string> WorkflowEngine::BuildExecutionPlan(
    const Workflow& workflow) {
  // Simple topological sort implementation
  // For now, return nodes in order (assumes no cycles)
  std::vector<std::string> plan;

  for (const auto& node : workflow.nodes) {
    plan.push_back(node.id);
  }

  return plan;
}

bool WorkflowEngine::HasCycle(const Workflow& workflow) const {
  // TODO: Implement cycle detection using DFS
  return false;
}

void WorkflowEngine::ExecuteNextNode(const std::string& execution_id) {
  auto it = executions_.find(execution_id);
  if (it == executions_.end()) {
    return;
  }

  ExecutionState* state = it->second.get();

  // Check if we're done
  if (state->current_node_index >= state->execution_order.size()) {
    CompleteExecution(execution_id);
    return;
  }

  // Get next node
  const std::string& node_id = state->execution_order[state->current_node_index];

  // Find node in workflow
  const WorkflowNode* node = nullptr;
  for (const auto& n : state->workflow.nodes) {
    if (n.id == node_id) {
      node = &n;
      break;
    }
  }

  if (!node) {
    FailExecution(execution_id, "Node not found: " + node_id);
    return;
  }

  // Update status
  state->context.status = WorkflowExecutionContext::Status::RUNNING;

  // Execute node
  ExecuteNode(execution_id, *node);
}

void WorkflowEngine::ExecuteNode(const std::string& execution_id,
                                 const WorkflowNode& node) {
  switch (node.type) {
    case NodeType::DATA_SOURCE:
      ExecuteDataSourceNode(execution_id, node);
      break;
    case NodeType::TRANSFORM:
      ExecuteTransformNode(execution_id, node);
      break;
    case NodeType::AI_PROCESSOR:
      ExecuteAIProcessorNode(execution_id, node);
      break;
    case NodeType::EXPORT:
      ExecuteExportNode(execution_id, node);
      break;
    case NodeType::FILTER:
      ExecuteFilterNode(execution_id, node);
      break;
    case NodeType::AGGREGATE:
      ExecuteAggregateNode(execution_id, node);
      break;
    case NodeType::SCRIPT:
      ExecuteScriptNode(execution_id, node);
      break;
    case NodeType::WEBHOOK:
      ExecuteWebhookNode(execution_id, node);
      break;
  }
}

void WorkflowEngine::ExecuteDataSourceNode(const std::string& execution_id,
                                           const WorkflowNode& node) {
  auto it = executions_.find(execution_id);
  if (it == executions_.end()) {
    return;
  }

  ExecutionState* state = it->second.get();

  // Get data source configuration
  std::string source_type;
  if (node.config.is_dict()) {
    if (const std::string* type = node.config.GetDict().FindString("source_type")) {
      source_type = *type;
    }
  }

  // Return all extracted data sources as a list
  // Workflows can filter/transform this data in subsequent nodes
  base::Value::List result_list;

  if (source_type.empty()) {
    // Return all data sources
    for (const auto& source : state->data_sources) {
      result_list.Append(source.Clone());
    }
  } else {
    // Filter by source type if specified
    for (const auto& source : state->data_sources) {
      if (source.is_dict()) {
        const std::string* type = source.GetDict().FindString("type");
        if (type && *type == source_type) {
          result_list.Append(source.Clone());
        }
      }
    }
  }

  LOG(INFO) << "Data source node returned " << result_list.size() << " sources";
  OnNodeComplete(execution_id, node.id, base::Value(std::move(result_list)));
}

void WorkflowEngine::ExecuteTransformNode(const std::string& execution_id,
                                         const WorkflowNode& node) {
  // Get input data from previous nodes
  base::Value input = GetNodeInput(execution_id, node);

  // Parse transform configuration
  TransformNode::TransformConfig config =
      TransformNode::TransformConfig::FromJson(node.config);

  // Execute transformation
  TransformNode transform_node;
  base::Value result = transform_node.Transform(input, config);

  // Complete with result
  OnNodeComplete(execution_id, node.id, std::move(result));
}

void WorkflowEngine::ExecuteAIProcessorNode(const std::string& execution_id,
                                           const WorkflowNode& node) {
  if (!url_loader_factory_) {
    OnNodeError(execution_id, node.id, "URL loader factory not available for AI processing");
    return;
  }

  // Get input data from previous nodes
  base::Value input = GetNodeInput(execution_id, node);

  // Parse AI configuration
  AIProcessorNode::AIConfig config =
      AIProcessorNode::AIConfig::FromJson(node.config);

  // Create AI processor node
  auto ai_node = std::make_unique<AIProcessorNode>(url_loader_factory_);

  // Process asynchronously
  AIProcessorNode* ai_node_ptr = ai_node.get();
  ai_node_ptr->Process(
      input, config,
      base::BindOnce(
          [](base::WeakPtr<WorkflowEngine> engine,
             std::string exec_id,
             std::string node_id,
             std::unique_ptr<AIProcessorNode> /* keep_alive */,
             bool success,
             base::Value result,
             std::string error) {
            if (!engine) {
              return;
            }
            if (success) {
              engine->OnNodeComplete(exec_id, node_id, std::move(result));
            } else {
              engine->OnNodeError(exec_id, node_id, error);
            }
          },
          weak_factory_.GetWeakPtr(), execution_id, node.id,
          std::move(ai_node)));
}

void WorkflowEngine::ExecuteExportNode(const std::string& execution_id,
                                      const WorkflowNode& node) {
  if (!url_loader_factory_) {
    OnNodeError(execution_id, node.id, "URL loader factory not available for export");
    return;
  }

  // Get input data from previous nodes
  base::Value input = GetNodeInput(execution_id, node);

  // Parse export configuration
  ExportNode::ExportConfig config;
  if (node.config.is_dict()) {
    const base::Value::Dict& dict = node.config.GetDict();
    if (const std::string* type = dict.FindString("export_type")) {
      config.export_type = *type;
    }
    if (const base::Value::Dict* settings = dict.FindDict("settings")) {
      config.settings = settings->Clone();
    }
  }

  // Create appropriate exporter based on type
  std::unique_ptr<ExportNode> exporter;
  if (config.export_type == "kafka") {
    exporter = std::make_unique<KafkaExporter>(url_loader_factory_);
  } else if (config.export_type == "redis") {
    exporter = std::make_unique<RedisExporter>(url_loader_factory_);
  } else if (config.export_type == "postgresql") {
    exporter = std::make_unique<PostgreSQLExporter>(url_loader_factory_);
  } else {
    OnNodeError(execution_id, node.id,
                "Unknown export type: " + config.export_type);
    return;
  }

  // Export asynchronously
  ExportNode* exporter_ptr = exporter.get();
  exporter_ptr->Export(
      input, config,
      base::BindOnce(
          [](base::WeakPtr<WorkflowEngine> engine,
             std::string exec_id,
             std::string node_id,
             std::unique_ptr<ExportNode> /* keep_alive */,
             bool success,
             std::string error) {
            if (!engine) {
              return;
            }
            if (success) {
              // Export successful, pass through the input data
              engine->OnNodeComplete(exec_id, node_id, base::Value());
            } else {
              engine->OnNodeError(exec_id, node_id, error);
            }
          },
          weak_factory_.GetWeakPtr(), execution_id, node.id,
          std::move(exporter)));
}

void WorkflowEngine::ExecuteFilterNode(const std::string& execution_id,
                                      const WorkflowNode& node) {
  // Get input data from previous nodes
  base::Value input = GetNodeInput(execution_id, node);

  // Parse filter configuration
  FilterNode::FilterConfig config =
      FilterNode::FilterConfig::FromJson(node.config);

  // Execute filter evaluation
  FilterNode filter_node;
  bool passed = filter_node.Evaluate(input, config);

  if (passed) {
    // Filter passed, pass data through
    OnNodeComplete(execution_id, node.id, std::move(input));
  } else {
    // Filter failed, return empty value (or could skip remaining nodes)
    LOG(INFO) << "Filter node " << node.id << " did not pass, stopping execution";
    OnNodeComplete(execution_id, node.id, base::Value());
  }
}

void WorkflowEngine::ExecuteAggregateNode(const std::string& execution_id,
                                         const WorkflowNode& node) {
  // TODO: Implement aggregation
  OnNodeComplete(execution_id, node.id, base::Value());
}

void WorkflowEngine::ExecuteScriptNode(const std::string& execution_id,
                                      const WorkflowNode& node) {
  // TODO: Implement script execution
  OnNodeComplete(execution_id, node.id, base::Value());
}

void WorkflowEngine::ExecuteWebhookNode(const std::string& execution_id,
                                       const WorkflowNode& node) {
  // TODO: Implement webhook call
  OnNodeComplete(execution_id, node.id, base::Value());
}

void WorkflowEngine::OnNodeComplete(const std::string& execution_id,
                                   const std::string& node_id,
                                   base::Value result) {
  auto it = executions_.find(execution_id);
  if (it == executions_.end()) {
    return;
  }

  ExecutionState* state = it->second.get();

  // Store node result
  state->context.node_results[node_id] = std::move(result);
  state->node_outputs[node_id] = state->context.node_results[node_id].Clone();

  // Move to next node
  state->current_node_index++;

  // Execute next node
  ExecuteNextNode(execution_id);
}

void WorkflowEngine::OnNodeError(const std::string& execution_id,
                                const std::string& node_id,
                                const std::string& error) {
  auto it = executions_.find(execution_id);
  if (it == executions_.end()) {
    return;
  }

  it->second->context.failed_node_id = node_id;
  FailExecution(execution_id, error);
}

void WorkflowEngine::CompleteExecution(const std::string& execution_id) {
  auto it = executions_.find(execution_id);
  if (it == executions_.end()) {
    return;
  }

  it->second->context.status = WorkflowExecutionContext::Status::COMPLETED;
  it->second->context.completed_at = base::Time::Now();

  std::move(it->second->callback).Run(it->second->context);
  executions_.erase(it);
}

void WorkflowEngine::FailExecution(const std::string& execution_id,
                                  const std::string& error) {
  auto it = executions_.find(execution_id);
  if (it == executions_.end()) {
    return;
  }

  it->second->context.status = WorkflowExecutionContext::Status::FAILED;
  it->second->context.error_message = error;
  it->second->context.completed_at = base::Time::Now();

  std::move(it->second->callback).Run(it->second->context);
  executions_.erase(it);
}

base::Value WorkflowEngine::GetNodeInput(const std::string& execution_id,
                                        const WorkflowNode& node) {
  auto it = executions_.find(execution_id);
  if (it == executions_.end()) {
    return base::Value();
  }

  // Find input edges for this node
  base::Value::List inputs;

  for (const auto& edge : it->second->workflow.edges) {
    if (edge.target_node_id == node.id) {
      // Get output from source node
      auto output_it = it->second->node_outputs.find(edge.source_node_id);
      if (output_it != it->second->node_outputs.end()) {
        inputs.Append(output_it->second.Clone());
      }
    }
  }

  return base::Value(std::move(inputs));
}

}  // namespace datasipper
