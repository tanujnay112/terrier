#pragma once

#include <memory>
#include <string>
#include "catalog/index_schema.h"
#include "catalog/schema.h"
#include "parser/expression/abstract_expression.h"
#include "storage/projected_row.h"
#include "storage/sql_table.h"
#include "storage/storage_defs.h"
#include "transaction/transaction_context.h"
#include "type/type_id.h"

namespace terrier::catalog::postgres {

constexpr table_oid_t PRO_TABLE_OID = table_oid_t(81);
constexpr index_oid_t PRO_OID_INDEX_OID = index_oid_t(82);
constexpr index_oid_t PRO_NAME_INDEX_OID = index_oid_t(83);

/*
 * Column names of the form "PRO[name]_COL_OID" are present in the PostgreSQL
 * catalog specification and columns of the form "ATT_[name]_COL_OID" are
 * terrier-specific addtions (generally pointers to internal objects).
 */
constexpr col_oid_t PROOID_COL_OID = col_oid_t(1);        // INTEGER (pkey) [proc_oid_t]
constexpr col_oid_t PRONAME_COL_OID = col_oid_t(2);       // VARCHAR (skey)
constexpr col_oid_t PRONAMESPACE_COL_OID = col_oid_t(3);  // INTEGER (skey) (fkey: pg_namespace) [namespace_oid_t]
constexpr col_oid_t PROLANG_COL_OID = col_oid_t(4);       // INTEGER (skey) (fkey: pg_language) [language_oid_t]
constexpr col_oid_t PROCOST_COL_OID = col_oid_t(5);       // DECIMAL (skey)
constexpr col_oid_t PROROWS_COL_OID = col_oid_t(6);       // DECIMAL (skey)
constexpr col_oid_t PROVARIADIC_COL_OID = col_oid_t(7);   // INTEGER (skey) (fkey: pg_type) [type_oid_t]

constexpr col_oid_t PROISAGG_COL_OID = col_oid_t(8);      // BOOLEAN (skey)
constexpr col_oid_t PROISWINDOW_COL_OID = col_oid_t(9);   // BOOLEAN (skey)
constexpr col_oid_t PROISSTRICT_COL_OID = col_oid_t(10);  // BOOLEAN (skey)
constexpr col_oid_t PRORETSET_COL_OID = col_oid_t(11);    // BOOLEAN (skey)
constexpr col_oid_t PROVOLATILE_COL_OID = col_oid_t(12);  // BOOLEAN (skey)

constexpr col_oid_t PRONARGS_COL_OID = col_oid_t(13);         // TINYINT (skey)
constexpr col_oid_t PRONARGDEFAULTS_COL_OID = col_oid_t(14);  // TINYINT (skey)
constexpr col_oid_t PRORETTYPE_COL_OID = col_oid_t(15);       // INTEGER (skey) (fkey: pg_type) [type_oid_t]
constexpr col_oid_t PROARGTYPES_COL_OID = col_oid_t(16);      // VARBINARY (skey) [type_oid_t[]]
constexpr col_oid_t PROALLARGTYPES_COL_OID = col_oid_t(17);   // VARBINARY (skey) [type_oid_t[]]

constexpr col_oid_t PROARGMODES_COL_OID = col_oid_t(18);  // VARCHAR (skey)
constexpr col_oid_t PROARGNAMES_COL_OID = col_oid_t(19);  // VARBINARY (skey) [text[]]

constexpr col_oid_t PROARGDEFAULTS_COL_OID = col_oid_t(20);  // BIGINT (assumes 64-bit pointers)

constexpr col_oid_t PROSRC_COL_OID = col_oid_t(21);  // VARCHAR (skey)

constexpr col_oid_t PROCONFIG_COL_OID = col_oid_t(22);  // VARBINARY (skey) [text[]]

constexpr uint8_t NUM_PG_PROC_COLS = 22;

constexpr std::array<col_oid_t, NUM_PG_PROC_COLS> PG_PRO_ALL_COL_OIDS = {
    PROOID_COL_OID,      PRONAME_COL_OID,        PRONAMESPACE_COL_OID, PROLANG_COL_OID,         PROCOST_COL_OID,
    PROROWS_COL_OID,     PROVARIADIC_COL_OID,    PROISAGG_COL_OID,     PROISWINDOW_COL_OID,     PROISSTRICT_COL_OID,
    PRORETSET_COL_OID,   PROVOLATILE_COL_OID,    PRONARGS_COL_OID,     PRONARGDEFAULTS_COL_OID, PRORETTYPE_COL_OID,
    PROARGTYPES_COL_OID, PROALLARGTYPES_COL_OID, PROARGMODES_COL_OID,  PROARGDEFAULTS_COL_OID,  PROARGNAMES_COL_OID,
    PROSRC_COL_OID,      PROCONFIG_COL_OID};

}  // namespace terrier::catalog::postgres