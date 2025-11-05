// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/workflow/workflow_storage.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "sql/statement.h"

namespace datasipper {

WorkflowStorage::WorkflowStorage(sql::Database* db) : db_(db) {
  DCHECK(db_);
}

WorkflowStorage::~WorkflowStorage() = default;

bool WorkflowStorage::Initialize() {
  if (!CreateWorkflowsTable()) {
    LOG(ERROR) << "Failed to create workflows table";
    return false;
  }

  if (!CreateExecutionsTable()) {
    LOG(ERROR) << "Failed to create executions table";
    return false;
  }

  return true;
}

bool WorkflowStorage::CreateWorkflowsTable() {
  const char kSql[] = R"(
    CREATE TABLE IF NOT EXISTS workflows (
      id TEXT PRIMARY KEY,
      name TEXT NOT NULL,
      description TEXT,
      url_pattern TEXT,
      workflow_json TEXT NOT NULL,
      status INTEGER DEFAULT 0,
      enabled INTEGER DEFAULT 1,
      created_at REAL NOT NULL,
      updated_at REAL NOT NULL,
      execution_count INTEGER DEFAULT 0,
      last_executed_at REAL
    )
  )";

  return db_->Execute(kSql);
}

bool WorkflowStorage::CreateExecutionsTable() {
  const char kSql[] = R"(
    CREATE TABLE IF NOT EXISTS workflow_executions (
      execution_id TEXT PRIMARY KEY,
      workflow_id TEXT NOT NULL,
      tab_id INTEGER,
      status INTEGER NOT NULL,
      started_at REAL NOT NULL,
      completed_at REAL,
      execution_json TEXT NOT NULL,
      FOREIGN KEY (workflow_id) REFERENCES workflows(id) ON DELETE CASCADE
    )
  )";

  if (!db_->Execute(kSql)) {
    return false;
  }

  // Create index on workflow_id for faster queries
  const char kIndexSql[] = R"(
    CREATE INDEX IF NOT EXISTS idx_executions_workflow_id
    ON workflow_executions(workflow_id)
  )";

  return db_->Execute(kIndexSql);
}

bool WorkflowStorage::SaveWorkflow(const Workflow& workflow) {
  const char kSql[] = R"(
    INSERT INTO workflows (
      id, name, description, url_pattern, workflow_json,
      status, enabled, created_at, updated_at, execution_count
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
  )";

  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindString(0, workflow.id);
  statement.BindString(1, workflow.name);
  statement.BindString(2, workflow.description);
  statement.BindString(3, workflow.url_pattern);
  statement.BindString(4, SerializeWorkflow(workflow));
  statement.BindInt(5, static_cast<int>(workflow.status));
  statement.BindBool(6, workflow.enabled);
  statement.BindDouble(7, workflow.created_at.InSecondsFSinceUnixEpoch());
  statement.BindDouble(8, workflow.updated_at.InSecondsFSinceUnixEpoch());
  statement.BindInt64(9, workflow.execution_count);

  return statement.Run();
}

bool WorkflowStorage::UpdateWorkflow(const Workflow& workflow) {
  const char kSql[] = R"(
    UPDATE workflows SET
      name = ?, description = ?, url_pattern = ?, workflow_json = ?,
      status = ?, enabled = ?, updated_at = ?, execution_count = ?
    WHERE id = ?
  )";

  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindString(0, workflow.name);
  statement.BindString(1, workflow.description);
  statement.BindString(2, workflow.url_pattern);
  statement.BindString(3, SerializeWorkflow(workflow));
  statement.BindInt(4, static_cast<int>(workflow.status));
  statement.BindBool(5, workflow.enabled);
  statement.BindDouble(6, workflow.updated_at.InSecondsFSinceUnixEpoch());
  statement.BindInt64(7, workflow.execution_count);
  statement.BindString(8, workflow.id);

  return statement.Run();
}

bool WorkflowStorage::DeleteWorkflow(const std::string& workflow_id) {
  const char kSql[] = "DELETE FROM workflows WHERE id = ?";
  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindString(0, workflow_id);
  return statement.Run();
}

Workflow WorkflowStorage::GetWorkflow(const std::string& workflow_id) const {
  const char kSql[] = "SELECT workflow_json FROM workflows WHERE id = ?";
  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindString(0, workflow_id);

  if (statement.Step()) {
    return DeserializeWorkflow(statement.ColumnString(0));
  }

  return Workflow();
}

std::vector<Workflow> WorkflowStorage::GetAllWorkflows() const {
  const char kSql[] = "SELECT workflow_json FROM workflows ORDER BY name";
  sql::Statement statement(db_->GetUniqueStatement(kSql));

  std::vector<Workflow> workflows;
  while (statement.Step()) {
    workflows.push_back(DeserializeWorkflow(statement.ColumnString(0)));
  }

  return workflows;
}

std::vector<Workflow> WorkflowStorage::GetWorkflowsForUrl(
    const std::string& url) const {
  // TODO: Implement regex matching for url_pattern
  const char kSql[] = R"(
    SELECT workflow_json FROM workflows
    WHERE enabled = 1 AND url_pattern != ''
    ORDER BY name
  )";

  sql::Statement statement(db_->GetUniqueStatement(kSql));

  std::vector<Workflow> workflows;
  while (statement.Step()) {
    workflows.push_back(DeserializeWorkflow(statement.ColumnString(0)));
  }

  return workflows;
}

std::vector<Workflow> WorkflowStorage::GetWorkflowsByStatus(
    Workflow::Status status) const {
  const char kSql[] = R"(
    SELECT workflow_json FROM workflows
    WHERE status = ?
    ORDER BY name
  )";

  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindInt(0, static_cast<int>(status));

  std::vector<Workflow> workflows;
  while (statement.Step()) {
    workflows.push_back(DeserializeWorkflow(statement.ColumnString(0)));
  }

  return workflows;
}

bool WorkflowStorage::SaveExecution(const WorkflowExecutionContext& execution) {
  const char kSql[] = R"(
    INSERT INTO workflow_executions (
      execution_id, workflow_id, tab_id, status,
      started_at, execution_json
    ) VALUES (?, ?, ?, ?, ?, ?)
  )";

  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindString(0, execution.execution_id);
  statement.BindString(1, execution.workflow_id);
  statement.BindInt(2, execution.tab_id);
  statement.BindInt(3, static_cast<int>(execution.status));
  statement.BindDouble(4, execution.started_at.InSecondsFSinceUnixEpoch());
  statement.BindString(5, SerializeExecution(execution));

  return statement.Run();
}

bool WorkflowStorage::UpdateExecution(
    const WorkflowExecutionContext& execution) {
  const char kSql[] = R"(
    UPDATE workflow_executions SET
      status = ?, completed_at = ?, execution_json = ?
    WHERE execution_id = ?
  )";

  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindInt(0, static_cast<int>(execution.status));

  if (!execution.completed_at.is_null()) {
    statement.BindDouble(1, execution.completed_at.InSecondsFSinceUnixEpoch());
  } else {
    statement.BindNull(1);
  }

  statement.BindString(2, SerializeExecution(execution));
  statement.BindString(3, execution.execution_id);

  return statement.Run();
}

WorkflowExecutionContext WorkflowStorage::GetExecution(
    const std::string& execution_id) const {
  const char kSql[] = R"(
    SELECT execution_json FROM workflow_executions
    WHERE execution_id = ?
  )";

  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindString(0, execution_id);

  if (statement.Step()) {
    return DeserializeExecution(statement.ColumnString(0));
  }

  return WorkflowExecutionContext();
}

std::vector<WorkflowExecutionContext> WorkflowStorage::GetExecutionsForWorkflow(
    const std::string& workflow_id,
    int limit) const {
  const char kSql[] = R"(
    SELECT execution_json FROM workflow_executions
    WHERE workflow_id = ?
    ORDER BY started_at DESC
    LIMIT ?
  )";

  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindString(0, workflow_id);
  statement.BindInt(1, limit);

  std::vector<WorkflowExecutionContext> executions;
  while (statement.Step()) {
    executions.push_back(DeserializeExecution(statement.ColumnString(0)));
  }

  return executions;
}

WorkflowStorage::ExecutionStats WorkflowStorage::GetExecutionStats(
    const std::string& workflow_id) const {
  ExecutionStats stats;

  const char kSql[] = R"(
    SELECT
      COUNT(*) as total,
      SUM(CASE WHEN status = 2 THEN 1 ELSE 0 END) as successful,
      SUM(CASE WHEN status = 3 THEN 1 ELSE 0 END) as failed,
      MAX(started_at) as last_execution
    FROM workflow_executions
    WHERE workflow_id = ?
  )";

  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindString(0, workflow_id);

  if (statement.Step()) {
    stats.total_executions = statement.ColumnInt64(0);
    stats.successful_executions = statement.ColumnInt64(1);
    stats.failed_executions = statement.ColumnInt64(2);

    double last_exec_time = statement.ColumnDouble(3);
    if (last_exec_time > 0) {
      stats.last_execution = base::Time::FromSecondsSinceUnixEpoch(last_exec_time);
    }
  }

  return stats;
}

bool WorkflowStorage::DeleteOldExecutions(int days_to_keep) {
  const char kSql[] = R"(
    DELETE FROM workflow_executions
    WHERE started_at < ?
  )";

  base::Time cutoff = base::Time::Now() - base::Days(days_to_keep);

  sql::Statement statement(db_->GetUniqueStatement(kSql));
  statement.BindDouble(0, cutoff.InSecondsFSinceUnixEpoch());

  return statement.Run();
}

base::Value WorkflowStorage::ExportWorkflow(
    const std::string& workflow_id) const {
  Workflow workflow = GetWorkflow(workflow_id);
  return workflow.ToJson();
}

bool WorkflowStorage::ImportWorkflow(const base::Value& workflow_json) {
  Workflow workflow = Workflow::FromJson(workflow_json);
  return SaveWorkflow(workflow);
}

std::string WorkflowStorage::SerializeWorkflow(const Workflow& workflow) const {
  std::string json;
  base::JSONWriter::Write(workflow.ToJson(), &json);
  return json;
}

Workflow WorkflowStorage::DeserializeWorkflow(const std::string& json) const {
  std::optional<base::Value> value = base::JSONReader::Read(json);
  if (!value) {
    return Workflow();
  }
  return Workflow::FromJson(*value);
}

std::string WorkflowStorage::SerializeExecution(
    const WorkflowExecutionContext& execution) const {
  std::string json;
  base::JSONWriter::Write(execution.ToJson(), &json);
  return json;
}

WorkflowExecutionContext WorkflowStorage::DeserializeExecution(
    const std::string& json) const {
  // TODO: Implement proper deserialization
  return WorkflowExecutionContext();
}

}  // namespace datasipper
