// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_WORKFLOW_WORKFLOW_STORAGE_H_
#define COMPONENTS_DATASIPPER_WORKFLOW_WORKFLOW_STORAGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/datasipper/workflow/workflow_definition.h"
#include "sql/database.h"

namespace datasipper {

// Persistent storage for workflows and execution history
class WorkflowStorage {
 public:
  explicit WorkflowStorage(sql::Database* db);
  ~WorkflowStorage();

  WorkflowStorage(const WorkflowStorage&) = delete;
  WorkflowStorage& operator=(const WorkflowStorage&) = delete;

  // Initialize storage (create tables)
  bool Initialize();

  // Workflow CRUD operations
  bool SaveWorkflow(const Workflow& workflow);
  bool UpdateWorkflow(const Workflow& workflow);
  bool DeleteWorkflow(const std::string& workflow_id);
  Workflow GetWorkflow(const std::string& workflow_id) const;
  std::vector<Workflow> GetAllWorkflows() const;

  // Query workflows by URL pattern
  std::vector<Workflow> GetWorkflowsForUrl(const std::string& url) const;

  // Query workflows by status
  std::vector<Workflow> GetWorkflowsByStatus(
      Workflow::Status status) const;

  // Execution history
  bool SaveExecution(const WorkflowExecutionContext& execution);
  bool UpdateExecution(const WorkflowExecutionContext& execution);
  WorkflowExecutionContext GetExecution(
      const std::string& execution_id) const;
  std::vector<WorkflowExecutionContext> GetExecutionsForWorkflow(
      const std::string& workflow_id,
      int limit = 100) const;

  // Get execution statistics
  struct ExecutionStats {
    int64_t total_executions = 0;
    int64_t successful_executions = 0;
    int64_t failed_executions = 0;
    int64_t average_duration_ms = 0;
    base::Time last_execution;
  };
  ExecutionStats GetExecutionStats(const std::string& workflow_id) const;

  // Cleanup old executions
  bool DeleteOldExecutions(int days_to_keep = 30);

  // Export/Import workflows
  base::Value ExportWorkflow(const std::string& workflow_id) const;
  bool ImportWorkflow(const base::Value& workflow_json);

 private:
  // Create database schema
  bool CreateWorkflowsTable();
  bool CreateExecutionsTable();

  // Serialize/deserialize workflows
  std::string SerializeWorkflow(const Workflow& workflow) const;
  Workflow DeserializeWorkflow(const std::string& json) const;

  // Serialize/deserialize execution context
  std::string SerializeExecution(
      const WorkflowExecutionContext& execution) const;
  WorkflowExecutionContext DeserializeExecution(
      const std::string& json) const;

  raw_ptr<sql::Database> db_;  // Not owned
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_WORKFLOW_WORKFLOW_STORAGE_H_
