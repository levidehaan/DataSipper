// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/datasipper/database/datasipper_database.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/datasipper/database/datasipper_database_schema.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace datasipper {

DataSipperDatabase::DataSipperDatabase(const base::FilePath& database_path)
    : database_path_(database_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

DataSipperDatabase::~DataSipperDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

sql::InitStatus DataSipperDatabase::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_.is_open()) {
    return sql::INIT_OK;
  }

  // Ensure the database directory exists
  if (!base::CreateDirectory(database_path_.DirName())) {
    DLOG(ERROR) << "Failed to create DataSipper database directory";
    return sql::INIT_FAILURE;
  }

  // Open the database
  if (!db_.Open(database_path_)) {
    DLOG(ERROR) << "Failed to open DataSipper database: " << database_path_;
    return sql::INIT_FAILURE;
  }

  // Configure SQLite settings
  db_.Preload();

  // Set pragmas for performance and reliability
  if (!db_.Execute("PRAGMA foreign_keys=ON") ||
      !db_.Execute("PRAGMA journal_mode=WAL") ||
      !db_.Execute("PRAGMA synchronous=NORMAL")) {
    DLOG(ERROR) << "Failed to set SQLite pragmas";
    return sql::INIT_FAILURE;
  }

  return InitializeSchema();
}

sql::InitStatus DataSipperDatabase::InitializeSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_.is_open());

  sql::MetaTable meta_table;
  if (!meta_table.Init(&db_, schema::kCurrentVersion, schema::kCompatibleVersion)) {
    DLOG(ERROR) << "Failed to initialize DataSipper meta table";
    return sql::INIT_FAILURE;
  }

  int current_version = meta_table.GetVersionNumber();
  if (current_version > schema::kCurrentVersion) {
    DLOG(ERROR) << "DataSipper database version too new: " << current_version;
    return sql::INIT_TOO_NEW;
  }

  // Create or upgrade schema
  if (current_version < schema::kCurrentVersion) {
    sql::Transaction transaction(&db_);
    if (!transaction.Begin()) {
      return sql::INIT_FAILURE;
    }

    if (current_version == 0) {
      // New database
      if (!CreateAllTables()) {
        return sql::INIT_FAILURE;
      }
    } else {
      // Upgrade existing database
      if (!schema::UpgradeSchema(&db_, current_version, schema::kCurrentVersion)) {
        DLOG(ERROR) << "Failed to upgrade DataSipper database schema";
        return sql::INIT_FAILURE;
      }
    }

    meta_table.SetVersionNumber(schema::kCurrentVersion);

    if (!transaction.Commit()) {
      return sql::INIT_FAILURE;
    }
  }

  // Create indexes
  if (!CreateIndexes()) {
    DLOG(WARNING) << "Failed to create some DataSipper database indexes";
    // Continue anyway - indexes are for performance, not correctness
  }

  return sql::INIT_OK;
}

bool DataSipperDatabase::CreateAllTables() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return schema::CreateAllTables(&db_);
}

bool DataSipperDatabase::CreateIndexes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return schema::CreateAllIndexes(&db_);
}

bool DataSipperDatabase::BeginTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_.BeginTransaction();
}

bool DataSipperDatabase::CommitTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_.CommitTransaction();
}

void DataSipperDatabase::RollbackTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_.RollbackTransaction();
}

bool DataSipperDatabase::Vacuum() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_.Execute("VACUUM");
}

bool DataSipperDatabase::ClearOldData(base::Time cutoff_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int64_t cutoff_microseconds = cutoff_time.ToInternalValue();

  sql::Statement statement_events(db_.GetUniqueStatement(
      "DELETE FROM network_events WHERE timestamp < ?"));
  statement_events.BindInt64(0, cutoff_microseconds);

  sql::Statement statement_messages(db_.GetUniqueStatement(
      "DELETE FROM websocket_messages WHERE timestamp < ?"));
  statement_messages.BindInt64(0, cutoff_microseconds);

  return statement_events.Run() && statement_messages.Run();
}

int64_t DataSipperDatabase::GetDatabaseSize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int64_t size = 0;
  if (base::GetFileSize(database_path_, &size)) {
    return size;
  }
  return -1;
}

}  // namespace datasipper
