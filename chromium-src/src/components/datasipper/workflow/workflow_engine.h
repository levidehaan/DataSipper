// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_WORKFLOW_WORKFLOW_ENGINE_H_
#define COMPONENTS_DATASIPPER_WORKFLOW_WORKFLOW_ENGINE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/datasipper/workflow/workflow_definition.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {
class WebContents;
}  // namespace content

namespace datasipper {

struct WorkflowNode;
class DataSourceNode;
class TransformNode;
class AIProcessorNode;
class ExportNode;
class KafkaExporter;
class RedisExporter;
class PostgreSQLExporter;

class WorkflowStorage;

// Main execution engine for workflows
class WorkflowEngine {
 public:
  using ExecutionCallback =
      base::OnceCallback<void(const WorkflowExecutionContext&)>;

  WorkflowEngine(WorkflowStorage* storage,
                 scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~WorkflowEngine();

  WorkflowEngine(const WorkflowEngine&) = delete;
  WorkflowEngine& operator=(const WorkflowEngine&) = delete;

  // Execute a workflow with optional extracted data sources
  void ExecuteWorkflow(const Workflow& workflow,
                       content::WebContents* web_contents,
                       ExecutionCallback callback,
                       const std::vector<base::Value>& data_sources = {});

  // Cancel a running execution
  void CancelExecution(const std::string& execution_id);

  // Get execution status
  WorkflowExecutionContext::Status GetExecutionStatus(
      const std::string& execution_id) const;

  // Get execution result
  WorkflowExecutionContext GetExecutionContext(
      const std::string& execution_id) const;

 private:
  // Execution state for a running workflow
  struct ExecutionState {
    ExecutionState();
    ~ExecutionState();

    WorkflowExecutionContext context;
    Workflow workflow;
    raw_ptr<content::WebContents> web_contents = nullptr;
    ExecutionCallback callback;

    // Node execution order (topologically sorted)
    std::vector<std::string> execution_order;

    // Current node being executed
    size_t current_node_index = 0;

    // Data flow between nodes
    std::map<std::string, base::Value> node_outputs;

    // Extracted data sources (from page detection)
    std::vector<base::Value> data_sources;
  };

  // Build execution plan (topological sort)
  std::vector<std::string> BuildExecutionPlan(const Workflow& workflow);

  // Detect cycles in workflow graph
  bool HasCycle(const Workflow& workflow) const;

  // Execute next node in sequence
  void ExecuteNextNode(const std::string& execution_id);

  // Execute a single node
  void ExecuteNode(const std::string& execution_id,
                   const WorkflowNode& node);

  // Node-specific execution methods
  void ExecuteDataSourceNode(const std::string& execution_id,
                             const WorkflowNode& node);
  void ExecuteTransformNode(const std::string& execution_id,
                           const WorkflowNode& node);
  void ExecuteAIProcessorNode(const std::string& execution_id,
                             const WorkflowNode& node);
  void ExecuteExportNode(const std::string& execution_id,
                        const WorkflowNode& node);
  void ExecuteFilterNode(const std::string& execution_id,
                        const WorkflowNode& node);
  void ExecuteAggregateNode(const std::string& execution_id,
                           const WorkflowNode& node);
  void ExecuteScriptNode(const std::string& execution_id,
                        const WorkflowNode& node);
  void ExecuteWebhookNode(const std::string& execution_id,
                         const WorkflowNode& node);

  // Handle node execution completion
  void OnNodeComplete(const std::string& execution_id,
                     const std::string& node_id,
                     base::Value result);

  // Handle node execution error
  void OnNodeError(const std::string& execution_id,
                  const std::string& node_id,
                  const std::string& error);

  // Complete workflow execution
  void CompleteExecution(const std::string& execution_id);

  // Fail workflow execution
  void FailExecution(const std::string& execution_id,
                    const std::string& error);

  // Get input data for a node (from connected nodes)
  base::Value GetNodeInput(const std::string& execution_id,
                          const WorkflowNode& node);

  // Active executions
  std::map<std::string, std::unique_ptr<ExecutionState>> executions_;

  // Workflow storage for persistence
  raw_ptr<WorkflowStorage> storage_;  // Not owned

  // URL loader factory for async HTTP requests (AI, exporters)
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<WorkflowEngine> weak_factory_{this};
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_WORKFLOW_WORKFLOW_ENGINE_H_
