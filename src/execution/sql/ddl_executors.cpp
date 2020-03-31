#include "execution/sql/ddl_executors.h"

#include <memory>
#include <string>
#include <vector>
#include <parser/udf/ast_nodes.h>
#include <parser/udf/udf_codegen.h>

#include "catalog/catalog_accessor.h"
#include "common/macros.h"
#include "execution/exec/execution_context.h"
#include "execution/compiler/function_builder.h"
#include "execution/compiler/codegen.h"
#include "parser/expression/column_value_expression.h"
#include "parser/udf/udf_ast_context.h"
#include "parser/udf/udf_parser.h"
#include "planner/plannodes/create_database_plan_node.h"
#include "planner/plannodes/create_index_plan_node.h"
#include "planner/plannodes/create_namespace_plan_node.h"
#include "planner/plannodes/create_table_plan_node.h"
#include "planner/plannodes/drop_database_plan_node.h"
#include "planner/plannodes/drop_index_plan_node.h"
#include "planner/plannodes/drop_namespace_plan_node.h"
#include "planner/plannodes/drop_table_plan_node.h"
#include "storage/index/index_builder.h"
#include "storage/sql_table.h"

namespace terrier::execution::sql {

bool DDLExecutors::CreateDatabaseExecutor(const common::ManagedPointer<planner::CreateDatabasePlanNode> node,
                                          const common::ManagedPointer<catalog::CatalogAccessor> accessor) {
  // Request permission from the Catalog to see if this a valid database name
  return accessor->CreateDatabase(node->GetDatabaseName()) != catalog::INVALID_DATABASE_OID;
}

bool DDLExecutors::CreateNamespaceExecutor(const common::ManagedPointer<planner::CreateNamespacePlanNode> node,
                                           const common::ManagedPointer<catalog::CatalogAccessor> accessor) {
  // Request permission from the Catalog to see if this a valid namespace name
  return accessor->CreateNamespace(node->GetNamespaceName()) != catalog::INVALID_NAMESPACE_OID;
}

bool DDLExecutors::CreateFunctionExecutor(const common::ManagedPointer<planner::CreateFunctionPlanNode> node,
                                           const common::ManagedPointer<exec::ExecutionContext> exec_ctx) {
  // Request permission from the Catalog to see if this a valid namespace name
  TERRIER_ASSERT(node->GetUDFLanguage() == parser::PLType::PL_PGSQL, "Unsupported language");
  TERRIER_ASSERT(node->GetFunctionBody().size() >= 1, "Unsupported function body?");

  // I don't like how we have to separate the two here
  std::vector<type::TypeId > param_type_ids;
  std::vector<catalog::type_oid_t> param_types;
  auto accessor = exec_ctx->GetAccessor();
  for(auto t : node->GetFunctionParameterTypes()){
    param_type_ids.push_back(parser::FuncParameter::DataTypeToTypeId(t));
    param_types.push_back(accessor->GetTypeOidFromTypeId(parser::FuncParameter::DataTypeToTypeId(t)));
  }
  auto body = node->GetFunctionBody().front();
  auto proc_id =  accessor->CreateProcedure(node->GetFunctionName(), catalog::postgres::PLPGSQL_LANGUAGE_OID,
      node->GetNamespaceOid(), node->GetFunctionParameterNames(), param_types,
      param_types, {},
      accessor->GetTypeOidFromTypeId(parser::ReturnType::DataTypeToTypeId(node->GetReturnType())),
      body, false);
  if(proc_id == catalog::INVALID_PROC_OID){
    return false;
  }

  // make the context here using the body
  parser::udf::UDFASTContext ast_context;
  // parser::udf::UDFContext udf_context;
  parser::udf::PLpgSQLParser udf_parser((common::ManagedPointer(&ast_context)));
  std::unique_ptr<parser::udf::FunctionAST> ast =
      udf_parser.ParsePLpgSQL(body, (common::ManagedPointer(&ast_context)));

  compiler::CodeGen codegen(exec_ctx.Get());
  util::RegionVector<ast::FieldDecl *> fn_params{codegen.Region()};
  for(size_t i = 0;i < node->GetFunctionParameterNames().size();i++) {
    auto name = node->GetFunctionParameterNames()[i];
    auto type = parser::ReturnType::DataTypeToTypeId(node->GetFunctionParameterTypes()[i]);
    auto name_alloc = reinterpret_cast<char*>(codegen.Region()->Allocate(name.length()+1));
    std::memcpy(name_alloc, name.c_str(), name.length() + 1);
    fn_params.emplace_back(codegen.MakeField(ast::Identifier{name_alloc}, codegen.TplType(type)));
  }

  auto name = node->GetFunctionName();
  char *name_alloc = reinterpret_cast<char*>(codegen.Region()->Allocate(name.length() + 1));
  std::memcpy(name_alloc, name.c_str(), name.length() + 1);

  compiler::FunctionBuilder fb{&codegen, ast::Identifier{name_alloc}, std::move(fn_params),
                               codegen.TplType(parser::ReturnType::DataTypeToTypeId(node->GetReturnType()))};
  parser::udf::UDFCodegen udf_codegen{&fb, nullptr, &codegen};
  udf_codegen.GenerateUDF(ast->body.get());
  auto fn = fb.Finish();
//  util::RegionVector<ast::Decl *> decls_reg_vec{decls->begin(), decls->end(), codegen.Region()};
  util::RegionVector<ast::Decl*> decls({fn}, codegen.Region());
  auto file = codegen.Compile(std::move(decls));

  udf::UDFContext *udf_context = new udf::UDFContext(node->GetFunctionName(),
      parser::ReturnType::DataTypeToTypeId(node->GetReturnType()), std::move(param_type_ids),
      common::ManagedPointer<ast::File>(file), codegen.ReleaseRegion());
  if(!accessor->SetProcCtxPtr(proc_id, udf_context)){
    delete udf_context;
    return false;
  }
  return true;
}

bool DDLExecutors::CreateTableExecutor(const common::ManagedPointer<planner::CreateTablePlanNode> node,
                                       const common::ManagedPointer<catalog::CatalogAccessor> accessor,
                                       const catalog::db_oid_t connection_db) {
  // Request permission from the Catalog to see if this a valid namespace and table name
  const auto table_oid = accessor->CreateTable(node->GetNamespaceOid(), node->GetTableName(), *(node->GetSchema()));
  if (table_oid == catalog::INVALID_TABLE_OID) {
    // Catalog wasn't able to proceed, txn must now abort
    return false;
  }
  // Get the canonical Schema from the Catalog now that column oids have been assigned
  const auto &schema = accessor->GetSchema(table_oid);
  // Instantiate a SqlTable and update the pointer in the Catalog
  auto *const table = new storage::SqlTable(node->GetBlockStore(), schema);
  bool result = accessor->SetTablePointer(table_oid, table);
  TERRIER_ASSERT(result, "CreateTable succeeded, SetTablePointer must also succeed.");

  if (node->HasPrimaryKey()) {
    // Create the IndexSchema Columns by referencing the Columns in the canonical table Schema from the Catalog
    std::vector<catalog::IndexSchema::Column> key_cols;
    const auto &primary_key_info = node->GetPrimaryKey();
    key_cols.reserve(primary_key_info.primary_key_cols_.size());
    for (const auto &parser_col : primary_key_info.primary_key_cols_) {
      const auto &table_col = schema.GetColumn(parser_col);
      if (table_col.Type() == type::TypeId::VARCHAR || table_col.Type() == type::TypeId::VARBINARY) {
        key_cols.emplace_back(table_col.Name(), table_col.Type(), table_col.MaxVarlenSize(), table_col.Nullable(),
                              parser::ColumnValueExpression(connection_db, table_oid, table_col.Oid()));

      } else {
        key_cols.emplace_back(table_col.Name(), table_col.Type(), table_col.Nullable(),
                              parser::ColumnValueExpression(connection_db, table_oid, table_col.Oid()));
      }
    }
    catalog::IndexSchema index_schema(key_cols, storage::index::IndexType::BWTREE, true, true, false, true);

    // Create the index, and use its return value as overall success result
    result = result &&
             CreateIndex(accessor, node->GetNamespaceOid(), primary_key_info.constraint_name_, table_oid, index_schema);
  }

  for (const auto &unique_constraint : node->GetUniqueConstraints()) {
    // TODO(Matt): we should add support in Catalog to update pg_constraint in this case
    // Create the IndexSchema Columns by referencing the Columns in the canonical table Schema from the Catalog
    std::vector<catalog::IndexSchema::Column> key_cols;
    for (const auto &unique_col : unique_constraint.unique_cols_) {
      const auto &table_col = schema.GetColumn(unique_col);
      if (table_col.Type() == type::TypeId::VARCHAR || table_col.Type() == type::TypeId::VARBINARY) {
        key_cols.emplace_back(table_col.Name(), table_col.Type(), table_col.MaxVarlenSize(), table_col.Nullable(),
                              parser::ColumnValueExpression(connection_db, table_oid, table_col.Oid()));

      } else {
        key_cols.emplace_back(table_col.Name(), table_col.Type(), table_col.Nullable(),
                              parser::ColumnValueExpression(connection_db, table_oid, table_col.Oid()));
      }
    }
    catalog::IndexSchema index_schema(key_cols, storage::index::IndexType::BWTREE, true, false, false, true);

    // Create the index, and use its return value as overall success result
    result = result && CreateIndex(accessor, node->GetNamespaceOid(), unique_constraint.constraint_name_, table_oid,
                                   index_schema);
  }

  // TODO(Matt): interpret other fields in CreateTablePlanNode when we support them in the Catalog:
  // foreign_keys_, con_checks_,

  return result;
}

bool DDLExecutors::CreateIndexExecutor(const common::ManagedPointer<planner::CreateIndexPlanNode> node,
                                       const common::ManagedPointer<catalog::CatalogAccessor> accessor) {
  return CreateIndex(accessor, node->GetNamespaceOid(), node->GetIndexName(), node->GetTableOid(),
                     *(node->GetSchema()));
}

bool DDLExecutors::DropDatabaseExecutor(const common::ManagedPointer<planner::DropDatabasePlanNode> node,
                                        const common::ManagedPointer<catalog::CatalogAccessor> accessor,
                                        const catalog::db_oid_t connection_db) {
  TERRIER_ASSERT(connection_db != node->GetDatabaseOid(),
                 "This command cannot be executed while connected to the target database. This should be checked in "
                 "the binder.");
  const bool result = accessor->DropDatabase(node->GetDatabaseOid());
  return result;
}

bool DDLExecutors::DropNamespaceExecutor(const common::ManagedPointer<planner::DropNamespacePlanNode> node,
                                         const common::ManagedPointer<catalog::CatalogAccessor> accessor) {
  const bool result = accessor->DropNamespace(node->GetNamespaceOid());
  // TODO(Matt): CASCADE?
  return result;
}

bool DDLExecutors::DropTableExecutor(const common::ManagedPointer<planner::DropTablePlanNode> node,
                                     const common::ManagedPointer<catalog::CatalogAccessor> accessor) {
  const bool result = accessor->DropTable(node->GetTableOid());
  // TODO(Matt): CASCADE?
  return result;
}

bool DDLExecutors::DropIndexExecutor(const common::ManagedPointer<planner::DropIndexPlanNode> node,
                                     const common::ManagedPointer<catalog::CatalogAccessor> accessor) {
  const bool result = accessor->DropIndex(node->GetIndexOid());
  // TODO(Matt): CASCADE?
  return result;
}

//bool DDLExecutors::DropFunctionExecutor(const common::ManagedPointer<planner::DropIndexPlanNode> node,
//                                     const common::ManagedPointer<catalog::CatalogAccessor> accessor) {
//  const bool result = accessor->DropIndex(node->GetIndexOid());
//  // TODO(Matt): CASCADE?
//  return result;
//}

bool DDLExecutors::CreateIndex(const common::ManagedPointer<catalog::CatalogAccessor> accessor,
                               const catalog::namespace_oid_t ns, const std::string &name,
                               const catalog::table_oid_t table, const catalog::IndexSchema &input_schema) {
  // Request permission from the Catalog to see if this a valid namespace and table name
  const auto index_oid = accessor->CreateIndex(ns, table, name, input_schema);
  if (index_oid == catalog::INVALID_INDEX_OID) {
    // Catalog wasn't able to proceed, txn must now abort
    return false;
  }
  // Get the canonical IndexSchema from the Catalog now that column oids have been assigned
  const auto &schema = accessor->GetIndexSchema(index_oid);
  // Instantiate an Index and update the pointer in the Catalog
  storage::index::IndexBuilder index_builder;
  index_builder.SetKeySchema(schema);
  auto *const index = index_builder.Build();
  bool result UNUSED_ATTRIBUTE = accessor->SetIndexPointer(index_oid, index);
  TERRIER_ASSERT(result, "CreateIndex succeeded, SetIndexPointer must also succeed.");
  return true;
}
}  // namespace terrier::execution::sql
