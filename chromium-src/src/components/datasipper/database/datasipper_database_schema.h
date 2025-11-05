// Copyright 2024 The DataSipper Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATASIPPER_DATABASE_DATASIPPER_DATABASE_SCHEMA_H_
#define COMPONENTS_DATASIPPER_DATABASE_DATASIPPER_DATABASE_SCHEMA_H_

#include <string>

namespace sql {
class Database;
}

namespace datasipper {

// SQL schema definitions for DataSipper database tables
namespace schema {

// Create all database tables
bool CreateNetworkEventsTable(sql::Database* db);
bool CreateWebSocketMessagesTable(sql::Database* db);
bool CreateConnectionsTable(sql::Database* db);
bool CreateConfigurationTable(sql::Database* db);

// Create indexes for performance
bool CreateIndexes(sql::Database* db);

// Table and column name constants
extern const char kNetworkEventsTable[];
extern const char kWebSocketMessagesTable[];
extern const char kConnectionsTable[];
extern const char kConfigurationTable[];

}  // namespace schema
}  // namespace datasipper

#endif  // COMPONENTS_DATASIPPER_DATABASE_DATASIPPER_DATABASE_SCHEMA_H_
