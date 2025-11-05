// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_DATABASE_SCHEMA_MANAGER_H_
#define COMPONENTS_DATASIPPER_DATABASE_SCHEMA_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "sql/database.h"

namespace datasipper {

// Manages dynamic database schema creation and modification
class SchemaManager {
 public:
  // Field type definitions
  enum class FieldType {
    TEXT,
    INTEGER,
    REAL,
    BLOB,
    BOOLEAN,
    TIMESTAMP
  };

  // Schema field definition
  struct FieldDefinition {
    FieldDefinition();
    FieldDefinition(const FieldDefinition&);
    FieldDefinition& operator=(const FieldDefinition&);
    ~FieldDefinition();

    std::string name;
    FieldType type;
    bool is_required = false;
    bool is_primary_key = false;
    bool is_unique = false;
    std::string default_value;
    std::string foreign_key_table;
    std::string foreign_key_column;
  };

  // Complete schema definition
  struct SchemaDefinition {
    SchemaDefinition();
    SchemaDefinition(const SchemaDefinition&);
    SchemaDefinition& operator=(const SchemaDefinition&);
    ~SchemaDefinition();

    std::string name;
    std::string table_name;
    std::vector<FieldDefinition> fields;
    std::vector<std::string> indexes;  // Field names to index
    base::Time created_at;
    base::Time updated_at;

    // Convert to/from JSON
    base::Value ToJson() const;
    static SchemaDefinition FromJson(const base::Value& json);
  };

  explicit SchemaManager(sql::Database* db);
  ~SchemaManager();

  SchemaManager(const SchemaManager&) = delete;
  SchemaManager& operator=(const SchemaManager&) = delete;

  // Initialize the schema manager (creates metadata tables)
  bool Initialize();

  // Schema CRUD operations
  bool CreateSchema(const SchemaDefinition& schema);
  bool UpdateSchema(const std::string& name, const SchemaDefinition& schema);
  bool DeleteSchema(const std::string& name);
  std::vector<SchemaDefinition> ListSchemas() const;
  SchemaDefinition GetSchema(const std::string& name) const;
  bool SchemaExists(const std::string& name) const;

  // Table operations
  bool TableExists(const std::string& table_name) const;
  bool DropTable(const std::string& table_name);

  // Data operations on custom tables
  bool InsertData(const std::string& table_name, const base::Value& data);
  bool UpdateData(const std::string& table_name,
                  const base::Value& data,
                  const std::string& where_clause);
  bool DeleteData(const std::string& table_name,
                  const std::string& where_clause);
  base::Value QueryData(const std::string& table_name,
                        const std::string& where_clause = "",
                        int limit = -1) const;

  // Utility methods
  static std::string FieldTypeToSql(FieldType type);
  static FieldType SqlToFieldType(const std::string& sql_type);

 private:
  // Generate CREATE TABLE SQL
  std::string GenerateCreateTableSql(const SchemaDefinition& schema) const;

  // Generate ALTER TABLE SQL for schema updates
  std::vector<std::string> GenerateAlterTableSql(
      const SchemaDefinition& old_schema,
      const SchemaDefinition& new_schema) const;

  // Save schema metadata to database
  bool SaveSchemaMetadata(const SchemaDefinition& schema);

  // Load schema metadata from database
  SchemaDefinition LoadSchemaMetadata(const std::string& name) const;

  raw_ptr<sql::Database> db_;  // Not owned
};

}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_DATABASE_SCHEMA_MANAGER_H_
