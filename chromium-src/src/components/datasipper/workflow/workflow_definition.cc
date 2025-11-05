// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/workflow/workflow_definition.h"

#include <set>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/uuid.h"

namespace datasipper {

// WorkflowNode implementation
WorkflowNode::WorkflowNode() = default;

WorkflowNode::WorkflowNode(const WorkflowNode& other)
    : id(other.id),
      type(other.type),
      label(other.label),
      config(other.config.Clone()),
      position(other.position) {}

WorkflowNode& WorkflowNode::operator=(const WorkflowNode& other) {
  if (this != &other) {
    id = other.id;
    type = other.type;
    label = other.label;
    config = other.config.Clone();
    position = other.position;
  }
  return *this;
}

WorkflowNode::~WorkflowNode() = default;

base::Value WorkflowNode::ToJson() const {
  base::Value::Dict dict;
  dict.Set("id", id);
  dict.Set("type", static_cast<int>(type));
  dict.Set("label", label);
  dict.Set("config", config.Clone());

  base::Value::Dict pos;
  pos.Set("x", position.x);
  pos.Set("y", position.y);
  dict.Set("position", std::move(pos));

  return base::Value(std::move(dict));
}

// static
WorkflowNode WorkflowNode::FromJson(const base::Value& json) {
  WorkflowNode node;

  if (!json.is_dict()) {
    return node;
  }

  const base::Value::Dict& dict = json.GetDict();

  if (const std::string* id_str = dict.FindString("id")) {
    node.id = *id_str;
  }

  if (std::optional<int> type_val = dict.FindInt("type")) {
    node.type = static_cast<NodeType>(*type_val);
  }

  if (const std::string* label_str = dict.FindString("label")) {
    node.label = *label_str;
  }

  if (const base::Value* config_val = dict.Find("config")) {
    node.config = config_val->Clone();
  }

  if (const base::Value::Dict* pos = dict.FindDict("position")) {
    if (std::optional<int> x = pos->FindInt("x")) {
      node.position.x = *x;
    }
    if (std::optional<int> y = pos->FindInt("y")) {
      node.position.y = *y;
    }
  }

  return node;
}

// WorkflowEdge implementation
WorkflowEdge::WorkflowEdge() = default;
WorkflowEdge::WorkflowEdge(const WorkflowEdge&) = default;
WorkflowEdge& WorkflowEdge::operator=(const WorkflowEdge&) = default;
WorkflowEdge::~WorkflowEdge() = default;

base::Value WorkflowEdge::ToJson() const {
  base::Value::Dict dict;
  dict.Set("id", id);
  dict.Set("source", source_node_id);
  dict.Set("target", target_node_id);

  if (!source_handle.empty()) {
    dict.Set("sourceHandle", source_handle);
  }
  if (!target_handle.empty()) {
    dict.Set("targetHandle", target_handle);
  }

  return base::Value(std::move(dict));
}

// static
WorkflowEdge WorkflowEdge::FromJson(const base::Value& json) {
  WorkflowEdge edge;

  if (!json.is_dict()) {
    return edge;
  }

  const base::Value::Dict& dict = json.GetDict();

  if (const std::string* id_str = dict.FindString("id")) {
    edge.id = *id_str;
  }
  if (const std::string* source = dict.FindString("source")) {
    edge.source_node_id = *source;
  }
  if (const std::string* target = dict.FindString("target")) {
    edge.target_node_id = *target;
  }
  if (const std::string* source_handle = dict.FindString("sourceHandle")) {
    edge.source_handle = *source_handle;
  }
  if (const std::string* target_handle = dict.FindString("targetHandle")) {
    edge.target_handle = *target_handle;
  }

  return edge;
}

// TriggerConfig implementation
TriggerConfig::TriggerConfig() = default;

TriggerConfig::TriggerConfig(const TriggerConfig& other)
    : type(other.type), config(other.config.Clone()) {}

TriggerConfig& TriggerConfig::operator=(const TriggerConfig& other) {
  if (this != &other) {
    type = other.type;
    config = other.config.Clone();
  }
  return *this;
}

TriggerConfig::~TriggerConfig() = default;

base::Value TriggerConfig::ToJson() const {
  base::Value::Dict dict;
  dict.Set("type", static_cast<int>(type));
  dict.Set("config", config.Clone());
  return base::Value(std::move(dict));
}

// static
TriggerConfig TriggerConfig::FromJson(const base::Value& json) {
  TriggerConfig trigger;

  if (!json.is_dict()) {
    return trigger;
  }

  const base::Value::Dict& dict = json.GetDict();

  if (std::optional<int> type_val = dict.FindInt("type")) {
    trigger.type = static_cast<TriggerType>(*type_val);
  }

  if (const base::Value* config_val = dict.Find("config")) {
    trigger.config = config_val->Clone();
  }

  return trigger;
}

// Workflow implementation
Workflow::Workflow() = default;

Workflow::Workflow(const Workflow& other)
    : id(other.id),
      name(other.name),
      description(other.description),
      url_pattern(other.url_pattern),
      nodes(other.nodes),
      edges(other.edges),
      trigger(other.trigger),
      auto_resume_on_tab_open(other.auto_resume_on_tab_open),
      enabled(other.enabled),
      created_at(other.created_at),
      updated_at(other.updated_at),
      execution_count(other.execution_count),
      last_executed_at(other.last_executed_at),
      status(other.status),
      output_schema_name(other.output_schema_name) {}

Workflow& Workflow::operator=(const Workflow& other) {
  if (this != &other) {
    id = other.id;
    name = other.name;
    description = other.description;
    url_pattern = other.url_pattern;
    nodes = other.nodes;
    edges = other.edges;
    trigger = other.trigger;
    auto_resume_on_tab_open = other.auto_resume_on_tab_open;
    enabled = other.enabled;
    created_at = other.created_at;
    updated_at = other.updated_at;
    execution_count = other.execution_count;
    last_executed_at = other.last_executed_at;
    status = other.status;
    output_schema_name = other.output_schema_name;
  }
  return *this;
}

Workflow::~Workflow() = default;

base::Value Workflow::ToJson() const {
  base::Value::Dict dict;

  dict.Set("id", id);
  dict.Set("name", name);
  dict.Set("description", description);
  dict.Set("url_pattern", url_pattern);

  // Nodes
  base::Value::List nodes_list;
  for (const auto& node : nodes) {
    nodes_list.Append(node.ToJson());
  }
  dict.Set("nodes", std::move(nodes_list));

  // Edges
  base::Value::List edges_list;
  for (const auto& edge : edges) {
    edges_list.Append(edge.ToJson());
  }
  dict.Set("edges", std::move(edges_list));

  // Trigger
  dict.Set("trigger", trigger.ToJson());

  // Settings
  dict.Set("auto_resume_on_tab_open", auto_resume_on_tab_open);
  dict.Set("enabled", enabled);
  dict.Set("status", static_cast<int>(status));

  // Metadata
  dict.Set("created_at", created_at.InSecondsFSinceUnixEpoch());
  dict.Set("updated_at", updated_at.InSecondsFSinceUnixEpoch());
  dict.Set("execution_count", base::NumberToString(execution_count));
  dict.Set("last_executed_at", last_executed_at.InSecondsFSinceUnixEpoch());

  if (!output_schema_name.empty()) {
    dict.Set("output_schema_name", output_schema_name);
  }

  return base::Value(std::move(dict));
}

// static
Workflow Workflow::FromJson(const base::Value& json) {
  Workflow workflow;

  if (!json.is_dict()) {
    return workflow;
  }

  const base::Value::Dict& dict = json.GetDict();

  if (const std::string* id_str = dict.FindString("id")) {
    workflow.id = *id_str;
  }
  if (const std::string* name_str = dict.FindString("name")) {
    workflow.name = *name_str;
  }
  if (const std::string* desc = dict.FindString("description")) {
    workflow.description = *desc;
  }
  if (const std::string* pattern = dict.FindString("url_pattern")) {
    workflow.url_pattern = *pattern;
  }

  // Nodes
  if (const base::Value::List* nodes_list = dict.FindList("nodes")) {
    for (const auto& node_val : *nodes_list) {
      workflow.nodes.push_back(WorkflowNode::FromJson(node_val));
    }
  }

  // Edges
  if (const base::Value::List* edges_list = dict.FindList("edges")) {
    for (const auto& edge_val : *edges_list) {
      workflow.edges.push_back(WorkflowEdge::FromJson(edge_val));
    }
  }

  // Trigger
  if (const base::Value* trigger_val = dict.Find("trigger")) {
    workflow.trigger = TriggerConfig::FromJson(*trigger_val);
  }

  // Settings
  if (std::optional<bool> auto_resume = dict.FindBool("auto_resume_on_tab_open")) {
    workflow.auto_resume_on_tab_open = *auto_resume;
  }
  if (std::optional<bool> enabled_val = dict.FindBool("enabled")) {
    workflow.enabled = *enabled_val;
  }
  if (std::optional<int> status_val = dict.FindInt("status")) {
    workflow.status = static_cast<Status>(*status_val);
  }

  // Metadata
  if (std::optional<double> created = dict.FindDouble("created_at")) {
    workflow.created_at = base::Time::FromSecondsSinceUnixEpoch(*created);
  }
  if (std::optional<double> updated = dict.FindDouble("updated_at")) {
    workflow.updated_at = base::Time::FromSecondsSinceUnixEpoch(*updated);
  }
  if (const std::string* exec_count = dict.FindString("execution_count")) {
    base::StringToInt64(*exec_count, &workflow.execution_count);
  }
  if (std::optional<double> last_exec = dict.FindDouble("last_executed_at")) {
    workflow.last_executed_at = base::Time::FromSecondsSinceUnixEpoch(*last_exec);
  }

  if (const std::string* schema_name = dict.FindString("output_schema_name")) {
    workflow.output_schema_name = *schema_name;
  }

  return workflow;
}

bool Workflow::IsValid() const {
  return GetValidationErrors().empty();
}

std::vector<std::string> Workflow::GetValidationErrors() const {
  std::vector<std::string> errors;

  if (id.empty()) {
    errors.push_back("Workflow ID is required");
  }

  if (name.empty()) {
    errors.push_back("Workflow name is required");
  }

  if (nodes.empty()) {
    errors.push_back("Workflow must have at least one node");
  }

  // Validate node IDs are unique
  std::set<std::string> node_ids;
  for (const auto& node : nodes) {
    if (node.id.empty()) {
      errors.push_back("All nodes must have an ID");
      continue;
    }
    if (node_ids.count(node.id)) {
      errors.push_back("Duplicate node ID: " + node.id);
    }
    node_ids.insert(node.id);
  }

  // Validate edges reference valid nodes
  for (const auto& edge : edges) {
    if (!node_ids.count(edge.source_node_id)) {
      errors.push_back("Edge references unknown source node: " + edge.source_node_id);
    }
    if (!node_ids.count(edge.target_node_id)) {
      errors.push_back("Edge references unknown target node: " + edge.target_node_id);
    }
  }

  return errors;
}

// WorkflowExecutionContext implementation
WorkflowExecutionContext::WorkflowExecutionContext() = default;

WorkflowExecutionContext::WorkflowExecutionContext(const WorkflowExecutionContext& other)
    : execution_id(other.execution_id),
      workflow_id(other.workflow_id),
      tab_id(other.tab_id),
      started_at(other.started_at),
      completed_at(other.completed_at),
      status(other.status),
      error_message(other.error_message),
      failed_node_id(other.failed_node_id) {
  for (const auto& [key, value] : other.node_results) {
    node_results[key] = value.Clone();
  }
}

WorkflowExecutionContext& WorkflowExecutionContext::operator=(const WorkflowExecutionContext& other) {
  if (this != &other) {
    execution_id = other.execution_id;
    workflow_id = other.workflow_id;
    tab_id = other.tab_id;
    started_at = other.started_at;
    completed_at = other.completed_at;
    status = other.status;
    error_message = other.error_message;
    failed_node_id = other.failed_node_id;
    node_results.clear();
    for (const auto& [key, value] : other.node_results) {
      node_results[key] = value.Clone();
    }
  }
  return *this;
}

WorkflowExecutionContext::WorkflowExecutionContext(WorkflowExecutionContext&&) = default;
WorkflowExecutionContext& WorkflowExecutionContext::operator=(WorkflowExecutionContext&&) = default;

WorkflowExecutionContext::~WorkflowExecutionContext() = default;

base::Value WorkflowExecutionContext::ToJson() const {
  base::Value::Dict dict;

  dict.Set("execution_id", execution_id);
  dict.Set("workflow_id", workflow_id);
  dict.Set("tab_id", tab_id);
  dict.Set("status", static_cast<int>(status));

  dict.Set("started_at", started_at.InSecondsFSinceUnixEpoch());
  if (!completed_at.is_null()) {
    dict.Set("completed_at", completed_at.InSecondsFSinceUnixEpoch());
  }

  // Node results
  base::Value::Dict results;
  for (const auto& [node_id, value] : node_results) {
    results.Set(node_id, value.Clone());
  }
  dict.Set("node_results", std::move(results));

  if (!error_message.empty()) {
    dict.Set("error_message", error_message);
  }
  if (!failed_node_id.empty()) {
    dict.Set("failed_node_id", failed_node_id);
  }

  return base::Value(std::move(dict));
}

}  // namespace datasipper
