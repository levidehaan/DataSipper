// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_WORKFLOW_WORKFLOW_DEFINITION_H_
#define COMPONENTS_DATASIPPER_WORKFLOW_WORKFLOW_DEFINITION_H_

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"

namespace datasipper {

// Types of workflow nodes
enum class NodeType {
  DATA_SOURCE,      // Reference to extracted page data
  TRANSFORM,        // Data transformation (JSONPath, regex, etc.)
  AI_PROCESSOR,     // LLM processing node
  EXPORT,           // Export to external system
  FILTER,           // Conditional filtering
  AGGREGATE,        // Aggregation operations
  SCRIPT,           // Custom JavaScript
  WEBHOOK           // HTTP webhook call
};

// Types of workflow triggers
enum class TriggerType {
  MANUAL,            // User clicks "Run"
  TIMER,             // Every N minutes/hours
  PAGE_LOAD,         // On page load/refresh
  PAGE_UNLOAD,       // Before page closes
  WEBSOCKET_EVENT,   // On WebSocket message matching filter
  DOM_MUTATION,      // On specific element change
  NETWORK_RESPONSE,  // On specific API call
  CUSTOM_EVENT       // Custom browser event
};

// Workflow node definition
struct WorkflowNode {
  WorkflowNode();
  WorkflowNode(const WorkflowNode&);
  WorkflowNode& operator=(const WorkflowNode&);
  ~WorkflowNode();

  std::string id;
  NodeType type;
  std::string label;
  base::Value config;  // Node-specific configuration (JSON)

  // Visual editor position
  struct Position {
    int x = 0;
    int y = 0;
  } position;

  // Convert to/from JSON
  base::Value ToJson() const;
  static WorkflowNode FromJson(const base::Value& json);
};

// Edge connecting two nodes
struct WorkflowEdge {
  WorkflowEdge();
  WorkflowEdge(const WorkflowEdge&);
  WorkflowEdge& operator=(const WorkflowEdge&);
  ~WorkflowEdge();

  std::string id;
  std::string source_node_id;
  std::string target_node_id;
  std::string source_handle;  // Optional: specific output port
  std::string target_handle;  // Optional: specific input port

  base::Value ToJson() const;
  static WorkflowEdge FromJson(const base::Value& json);
};

// Trigger configuration
struct TriggerConfig {
  TriggerConfig();
  TriggerConfig(const TriggerConfig&);
  TriggerConfig& operator=(const TriggerConfig&);
  ~TriggerConfig();

  TriggerType type;
  base::Value config;  // Type-specific settings

  // For TIMER trigger
  struct TimerConfig {
    int interval_minutes = 5;
    bool run_immediately = false;
  };

  // For WEBSOCKET_EVENT trigger
  struct WebSocketConfig {
    std::string event_filter;  // JSONPath or regex
    bool match_any = true;
  };

  // For DOM_MUTATION trigger
  struct DomMutationConfig {
    std::string selector;
    bool watch_attributes = true;
    bool watch_children = true;
    bool watch_text = true;
  };

  base::Value ToJson() const;
  static TriggerConfig FromJson(const base::Value& json);
};

// Complete workflow definition
struct Workflow {
  Workflow();
  Workflow(const Workflow&);
  Workflow& operator=(const Workflow&);
  ~Workflow();

  std::string id;
  std::string name;
  std::string description;

  // URL pattern to match against tabs (regex)
  std::string url_pattern;

  // Workflow graph
  std::vector<WorkflowNode> nodes;
  std::vector<WorkflowEdge> edges;

  // Execution settings
  TriggerConfig trigger;
  bool auto_resume_on_tab_open = true;
  bool enabled = true;

  // Metadata
  base::Time created_at;
  base::Time updated_at;
  int64_t execution_count = 0;
  base::Time last_executed_at;

  // Workflow state
  enum class Status {
    ACTIVE,      // Currently running
    PAUSED,      // Temporarily disabled
    STOPPED,     // Permanently stopped
    ERROR        // Last execution failed
  };
  Status status = Status::PAUSED;

  // Schema association (for exporting to custom tables)
  std::string output_schema_name;

  // Convert to/from JSON for storage
  base::Value ToJson() const;
  static Workflow FromJson(const base::Value& json);

  // Validate workflow structure
  bool IsValid() const;
  std::vector<std::string> GetValidationErrors() const;
};

// Execution context for a workflow run
struct WorkflowExecutionContext {
  WorkflowExecutionContext();
  WorkflowExecutionContext(const WorkflowExecutionContext&);
  WorkflowExecutionContext& operator=(const WorkflowExecutionContext&);
  WorkflowExecutionContext(WorkflowExecutionContext&&);
  WorkflowExecutionContext& operator=(WorkflowExecutionContext&&);
  ~WorkflowExecutionContext();

  std::string execution_id;
  std::string workflow_id;
  int tab_id = -1;

  base::Time started_at;
  base::Time completed_at;

  enum class Status {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED
  };
  Status status = Status::PENDING;

  // Execution results per node
  std::map<std::string, base::Value> node_results;

  // Error information
  std::string error_message;
  std::string failed_node_id;

  base::Value ToJson() const;
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_WORKFLOW_WORKFLOW_DEFINITION_H_
