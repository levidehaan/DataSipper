// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/database/schema_manager.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace datasipper {

namespace {
// Helper to execute SQL without cstring_view issues
bool ExecuteSql(sql::Database* db, const std::string& sql) {
  sql::Statement stmt(db->GetUniqueStatement(sql));
  return stmt.Run();
}
}  // namespace

// FieldDefinition implementation
SchemaManager::FieldDefinition::FieldDefinition() = default;
SchemaManager::FieldDefinition::FieldDefinition(const FieldDefinition&) = default;
SchemaManager::FieldDefinition& SchemaManager::FieldDefinition::operator=(
    const FieldDefinition&) = default;
SchemaManager::FieldDefinition::~FieldDefinition() = default;

// SchemaDefinition implementation
SchemaManager::SchemaDefinition::SchemaDefinition() = default;
SchemaManager::SchemaDefinition::SchemaDefinition(const SchemaDefinition&) = default;
SchemaManager::SchemaDefinition& SchemaManager::SchemaDefinition::operator=(
    const SchemaDefinition&) = default;
SchemaManager::SchemaDefinition::~SchemaDefinition() = default;

base::Value SchemaManager::SchemaDefinition::ToJson() const {
  base::Value::Dict dict;
  dict.Set("name", name);
  dict.Set("table_name", table_name);

  base::Value::List fields_list;
  for (const auto& field : fields) {
    base::Value::Dict field_dict;
    field_dict.Set("name", field.name);
    field_dict.Set("type", static_cast<int>(field.type));
    field_dict.Set("is_required", field.is_required);
    field_dict.Set("is_primary_key", field.is_primary_key);
    field_dict.Set("is_unique", field.is_unique);
    if (!field.default_value.empty()) {
      field_dict.Set("default_value", field.default_value);
    }
    if (!field.foreign_key_table.empty()) {
      field_dict.Set("foreign_key_table", field.foreign_key_table);
      field_dict.Set("foreign_key_column", field.foreign_key_column);
    }
    fields_list.Append(std::move(field_dict));
  }
  dict.Set("fields", std::move(fields_list));

  base::Value::List indexes_list;
  for (const auto& index : indexes) {
    indexes_list.Append(index);
  }
  dict.Set("indexes", std::move(indexes_list));

  return base::Value(std::move(dict));
}

SchemaManager::SchemaDefinition SchemaManager::SchemaDefinition::FromJson(
    const base::Value& json) {
  SchemaDefinition schema;

  if (!json.is_dict()) {
    return schema;
  }

  const base::Value::Dict& dict = json.GetDict();

  if (const std::string* name_ptr = dict.FindString("name")) {
    schema.name = *name_ptr;
  }

  if (const std::string* table_name_ptr = dict.FindString("table_name")) {
    schema.table_name = *table_name_ptr;
  }

  if (const base::Value::List* fields_list = dict.FindList("fields")) {
    for (const auto& field_value : *fields_list) {
      if (!field_value.is_dict()) {
        continue;
      }

      const base::Value::Dict& field_dict = field_value.GetDict();
      FieldDefinition field;

      if (const std::string* field_name = field_dict.FindString("name")) {
        field.name = *field_name;
      }

      if (auto type_int = field_dict.FindInt("type")) {
        field.type = static_cast<FieldType>(*type_int);
      }

      field.is_required = field_dict.FindBool("is_required").value_or(false);
      field.is_primary_key = field_dict.FindBool("is_primary_key").value_or(false);
      field.is_unique = field_dict.FindBool("is_unique").value_or(false);

      if (const std::string* default_val = field_dict.FindString("default_value")) {
        field.default_value = *default_val;
      }

      if (const std::string* fk_table = field_dict.FindString("foreign_key_table")) {
        field.foreign_key_table = *fk_table;
      }

      if (const std::string* fk_column = field_dict.FindString("foreign_key_column")) {
        field.foreign_key_column = *fk_column;
      }

      schema.fields.push_back(std::move(field));
    }
  }

  if (const base::Value::List* indexes_list = dict.FindList("indexes")) {
    for (const auto& index_value : *indexes_list) {
      if (const std::string* index_str = index_value.GetIfString()) {
        schema.indexes.push_back(*index_str);
      }
    }
  }

  return schema;
}

// SchemaManager implementation
SchemaManager::SchemaManager(sql::Database* db) : db_(db) {}
SchemaManager::~SchemaManager() = default;

bool SchemaManager::Initialize() {
  if (!db_) {
    return false;
  }

  // Create schema metadata table
  const char kCreateSchemasTable[] = R"(
    CREATE TABLE IF NOT EXISTS datasipper_schemas (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      name TEXT UNIQUE NOT NULL,
      table_name TEXT UNIQUE NOT NULL,
      schema_json TEXT NOT NULL,
      created_at INTEGER NOT NULL,
      updated_at INTEGER NOT NULL
    )
  )";

  if (!ExecuteSql(db_, kCreateSchemasTable)) {
    LOG(ERROR) << "Failed to create datasipper_schemas table";
    return false;
  }

  LOG(INFO) << "SchemaManager initialized successfully";
  return true;
}

bool SchemaManager::CreateSchema(const SchemaDefinition& schema) {
  if (!db_) {
    return false;
  }

  if (schema.name.empty() || schema.table_name.empty()) {
    LOG(ERROR) << "Schema name and table_name are required";
    return false;
  }

  if (schema.fields.empty()) {
    LOG(ERROR) << "Schema must have at least one field";
    return false;
  }

  // Check if schema already exists
  if (SchemaExists(schema.name)) {
    LOG(ERROR) << "Schema already exists: " << schema.name;
    return false;
  }

  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  // Generate and execute CREATE TABLE
  std::string create_sql = GenerateCreateTableSql(schema);
  if (!ExecuteSql(db_, create_sql)) {
    LOG(ERROR) << "Failed to create table: " << schema.table_name;
    return false;
  }

  // Create indexes
  for (const auto& index_field : schema.indexes) {
    std::string index_sql = base::StrCat({
        "CREATE INDEX IF NOT EXISTS idx_", schema.table_name, "_",
        index_field, " ON ", schema.table_name, "(", index_field, ")"});

    if (!ExecuteSql(db_, index_sql)) {
      LOG(WARNING) << "Failed to create index on " << index_field;
    }
  }

  // Save schema metadata
  if (!SaveSchemaMetadata(schema)) {
    LOG(ERROR) << "Failed to save schema metadata";
    return false;
  }

  if (!transaction.Commit()) {
    return false;
  }

  LOG(INFO) << "Created schema: " << schema.name << " (table: " << schema.table_name << ")";
  return true;
}

bool SchemaManager::UpdateSchema(const std::string& name,
                                 const SchemaDefinition& new_schema) {
  if (!db_) {
    return false;
  }

  SchemaDefinition old_schema = GetSchema(name);
  if (old_schema.name.empty()) {
    LOG(ERROR) << "Schema not found: " << name;
    return false;
  }

  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  // Generate ALTER TABLE statements
  std::vector<std::string> alter_sqls = GenerateAlterTableSql(old_schema, new_schema);

  for (const auto& sql_str : alter_sqls) {
    if (!ExecuteSql(db_, sql_str)) {
      LOG(ERROR) << "Failed to execute: " << sql_str;
      return false;
    }
  }

  // Update schema metadata
  if (!SaveSchemaMetadata(new_schema)) {
    return false;
  }

  if (!transaction.Commit()) {
    return false;
  }

  LOG(INFO) << "Updated schema: " << name;
  return true;
}

bool SchemaManager::DeleteSchema(const std::string& name) {
  if (!db_) {
    return false;
  }

  SchemaDefinition schema = GetSchema(name);
  if (schema.name.empty()) {
    LOG(ERROR) << "Schema not found: " << name;
    return false;
  }

  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  // Drop the table
  std::string drop_sql = base::StrCat({"DROP TABLE IF EXISTS ", schema.table_name});
  if (!ExecuteSql(db_, drop_sql)) {
    LOG(ERROR) << "Failed to drop table: " << schema.table_name;
    return false;
  }

  // Delete schema metadata
  sql::Statement delete_stmt(db_->GetUniqueStatement(
      "DELETE FROM datasipper_schemas WHERE name = ?"));
  delete_stmt.BindString(0, name);

  if (!delete_stmt.Run()) {
    return false;
  }

  if (!transaction.Commit()) {
    return false;
  }

  LOG(INFO) << "Deleted schema: " << name;
  return true;
}

std::vector<SchemaManager::SchemaDefinition> SchemaManager::ListSchemas() const {
  std::vector<SchemaDefinition> schemas;

  if (!db_) {
    return schemas;
  }

  sql::Statement stmt(db_->GetUniqueStatement(
      "SELECT name FROM datasipper_schemas ORDER BY name"));

  while (stmt.Step()) {
    std::string name = stmt.ColumnString(0);
    SchemaDefinition schema = GetSchema(name);
    if (!schema.name.empty()) {
      schemas.push_back(std::move(schema));
    }
  }

  return schemas;
}

SchemaManager::SchemaDefinition SchemaManager::GetSchema(const std::string& name) const {
  return LoadSchemaMetadata(name);
}

bool SchemaManager::SchemaExists(const std::string& name) const {
  if (!db_) {
    return false;
  }

  sql::Statement stmt(db_->GetUniqueStatement(
      "SELECT COUNT(*) FROM datasipper_schemas WHERE name = ?"));
  stmt.BindString(0, name);

  if (stmt.Step()) {
    return stmt.ColumnInt(0) > 0;
  }

  return false;
}

bool SchemaManager::TableExists(const std::string& table_name) const {
  if (!db_) {
    return false;
  }

  sql::Statement stmt(db_->GetUniqueStatement(
      "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?"));
  stmt.BindString(0, table_name);

  if (stmt.Step()) {
    return stmt.ColumnInt(0) > 0;
  }

  return false;
}

bool SchemaManager::DropTable(const std::string& table_name) {
  if (!db_) {
    return false;
  }

  std::string sql = base::StrCat({"DROP TABLE IF EXISTS ", table_name});
  return ExecuteSql(db_, sql);
}

bool SchemaManager::InsertData(const std::string& table_name,
                               const base::Value& data) {
  if (!db_ || !data.is_dict()) {
    return false;
  }

  const base::Value::Dict& dict = data.GetDict();

  std::vector<std::string> columns;
  std::vector<std::string> placeholders;

  for (const auto [key, value] : dict) {
    columns.push_back(key);
    placeholders.push_back("?");
  }

  std::string sql = base::StrCat({
      "INSERT INTO ", table_name, " (",
      base::JoinString(columns, ", "), ") VALUES (",
      base::JoinString(placeholders, ", "), ")"});

  sql::Statement stmt(db_->GetUniqueStatement(sql));

  int param_index = 0;
  for (const auto [key, value] : dict) {
    if (value.is_string()) {
      stmt.BindString(param_index, value.GetString());
    } else if (value.is_int()) {
      stmt.BindInt64(param_index, value.GetInt());
    } else if (value.is_double()) {
      stmt.BindDouble(param_index, value.GetDouble());
    } else if (value.is_bool()) {
      stmt.BindBool(param_index, value.GetBool());
    } else {
      // For complex types, store as JSON
      std::string json_str;
      base::JSONWriter::Write(value, &json_str);
      stmt.BindString(param_index, json_str);
    }
    param_index++;
  }

  return stmt.Run();
}

bool SchemaManager::UpdateData(const std::string& table_name,
                               const base::Value& data,
                               const std::string& where_clause) {
  if (!db_ || !data.is_dict()) {
    return false;
  }

  const base::Value::Dict& dict = data.GetDict();

  std::vector<std::string> set_clauses;
  for (const auto [key, value] : dict) {
    set_clauses.push_back(key + " = ?");
  }

  std::string sql = base::StrCat({
      "UPDATE ", table_name, " SET ",
      base::JoinString(set_clauses, ", ")});

  if (!where_clause.empty()) {
    sql = base::StrCat({sql, " WHERE ", where_clause});
  }

  sql::Statement stmt(db_->GetUniqueStatement(sql));

  int param_index = 0;
  for (const auto [key, value] : dict) {
    if (value.is_string()) {
      stmt.BindString(param_index, value.GetString());
    } else if (value.is_int()) {
      stmt.BindInt64(param_index, value.GetInt());
    } else if (value.is_double()) {
      stmt.BindDouble(param_index, value.GetDouble());
    } else if (value.is_bool()) {
      stmt.BindBool(param_index, value.GetBool());
    }
    param_index++;
  }

  return stmt.Run();
}

bool SchemaManager::DeleteData(const std::string& table_name,
                               const std::string& where_clause) {
  if (!db_) {
    return false;
  }

  std::string sql = base::StrCat({"DELETE FROM ", table_name});

  if (!where_clause.empty()) {
    sql = base::StrCat({sql, " WHERE ", where_clause});
  }

  sql::Statement stmt(db_->GetUniqueStatement(sql));
  return stmt.Run();
}

base::Value SchemaManager::QueryData(const std::string& table_name,
                                     const std::string& where_clause,
                                     int limit) const {
  base::Value::List results;

  if (!db_) {
    return base::Value(std::move(results));
  }

  std::string sql = base::StrCat({"SELECT * FROM ", table_name});

  if (!where_clause.empty()) {
    sql = base::StrCat({sql, " WHERE ", where_clause});
  }

  if (limit > 0) {
    sql = base::StrCat({sql, " LIMIT ", base::NumberToString(limit)});
  }

  sql::Statement stmt(db_->GetUniqueStatement(sql));

  while (stmt.Step()) {
    base::Value::Dict row;

    for (int i = 0; i < stmt.ColumnCount(); i++) {
      // Use column index for keys since ColumnName() doesn't exist
      std::string col_name = base::StrCat({"col_", base::NumberToString(i)});

      switch (stmt.GetColumnType(i)) {
        case sql::ColumnType::kInteger:
          row.Set(col_name, static_cast<int>(stmt.ColumnInt64(i)));
          break;
        case sql::ColumnType::kFloat:
          row.Set(col_name, stmt.ColumnDouble(i));
          break;
        case sql::ColumnType::kText:
          row.Set(col_name, stmt.ColumnString(i));
          break;
        case sql::ColumnType::kBlob:
          // For blobs, we could base64 encode, but skip for now
          row.Set(col_name, std::string("BLOB"));
          break;
        case sql::ColumnType::kNull:
          // Skip null values
          break;
      }
    }

    results.Append(std::move(row));
  }

  return base::Value(std::move(results));
}

std::string SchemaManager::FieldTypeToSql(FieldType type) {
  switch (type) {
    case FieldType::TEXT:
      return "TEXT";
    case FieldType::INTEGER:
      return "INTEGER";
    case FieldType::REAL:
      return "REAL";
    case FieldType::BLOB:
      return "BLOB";
    case FieldType::BOOLEAN:
      return "INTEGER";  // SQLite uses INTEGER for booleans
    case FieldType::TIMESTAMP:
      return "INTEGER";  // SQLite uses INTEGER for timestamps
  }
  return "TEXT";
}

SchemaManager::FieldType SchemaManager::SqlToFieldType(const std::string& sql_type) {
  std::string upper = base::ToUpperASCII(sql_type);

  if (upper.find("INT") != std::string::npos) {
    return FieldType::INTEGER;
  } else if (upper.find("REAL") != std::string::npos ||
             upper.find("FLOAT") != std::string::npos ||
             upper.find("DOUBLE") != std::string::npos) {
    return FieldType::REAL;
  } else if (upper.find("BLOB") != std::string::npos) {
    return FieldType::BLOB;
  } else {
    return FieldType::TEXT;
  }
}

std::string SchemaManager::GenerateCreateTableSql(const SchemaDefinition& schema) const {
  std::vector<std::string> column_defs;

  for (const auto& field : schema.fields) {
    std::string col_def = field.name + " " + FieldTypeToSql(field.type);

    if (field.is_primary_key) {
      col_def += " PRIMARY KEY";
    }

    if (field.is_required && !field.is_primary_key) {
      col_def += " NOT NULL";
    }

    if (field.is_unique && !field.is_primary_key) {
      col_def += " UNIQUE";
    }

    if (!field.default_value.empty()) {
      col_def += " DEFAULT " + field.default_value;
    }

    column_defs.push_back(col_def);
  }

  // Add foreign keys
  for (const auto& field : schema.fields) {
    if (!field.foreign_key_table.empty()) {
      std::string fk_def = base::StrCat({
          "FOREIGN KEY(", field.name, ") REFERENCES ",
          field.foreign_key_table, "(", field.foreign_key_column, ")"});
      column_defs.push_back(fk_def);
    }
  }

  return base::StrCat({
      "CREATE TABLE ", schema.table_name, " (\n  ",
      base::JoinString(column_defs, ",\n  "), "\n)"});
}

std::vector<std::string> SchemaManager::GenerateAlterTableSql(
    const SchemaDefinition& old_schema,
    const SchemaDefinition& new_schema) const {
  std::vector<std::string> sqls;

  // Find new fields
  for (const auto& new_field : new_schema.fields) {
    bool found = false;
    for (const auto& old_field : old_schema.fields) {
      if (old_field.name == new_field.name) {
        found = true;
        break;
      }
    }

    if (!found) {
      // Add new column
      std::string alter_sql = base::StrCat({
          "ALTER TABLE ", new_schema.table_name,
          " ADD COLUMN ", new_field.name, " ",
          FieldTypeToSql(new_field.type)});

      if (!new_field.default_value.empty()) {
        alter_sql = base::StrCat({alter_sql, " DEFAULT ", new_field.default_value});
      }

      sqls.push_back(alter_sql);
    }
  }

  // Note: SQLite doesn't support DROP COLUMN easily, would need table recreation

  return sqls;
}

bool SchemaManager::SaveSchemaMetadata(const SchemaDefinition& schema) {
  if (!db_) {
    return false;
  }

  std::string schema_json;
  base::JSONWriter::Write(schema.ToJson(), &schema_json);

  base::Time now = base::Time::Now();

  // Try insert first
  sql::Statement insert_stmt(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO datasipper_schemas "
      "(name, table_name, schema_json, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?)"));

  insert_stmt.BindString(0, schema.name);
  insert_stmt.BindString(1, schema.table_name);
  insert_stmt.BindString(2, schema_json);
  insert_stmt.BindInt64(3, now.ToInternalValue());
  insert_stmt.BindInt64(4, now.ToInternalValue());

  return insert_stmt.Run();
}

SchemaManager::SchemaDefinition SchemaManager::LoadSchemaMetadata(
    const std::string& name) const {
  SchemaDefinition schema;

  if (!db_) {
    return schema;
  }

  sql::Statement stmt(db_->GetUniqueStatement(
      "SELECT schema_json, created_at, updated_at FROM datasipper_schemas "
      "WHERE name = ?"));
  stmt.BindString(0, name);

  if (!stmt.Step()) {
    return schema;
  }

  std::string schema_json = stmt.ColumnString(0);
  auto parsed = base::JSONReader::Read(schema_json);

  if (parsed && parsed->is_dict()) {
    schema = SchemaDefinition::FromJson(*parsed);
    schema.created_at = base::Time::FromInternalValue(stmt.ColumnInt64(1));
    schema.updated_at = base::Time::FromInternalValue(stmt.ColumnInt64(2));
  }

  return schema;
}

}  // namespace datasipper
