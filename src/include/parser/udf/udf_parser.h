#include <memory>
#include <vector>

#pragma once

#include "nlohmann/json.hpp"
#include "ast_nodes.h"

#include "parser/expression_util.h"

namespace terrier::parser::udf {


class FunctionAST;
class PLpgSQLParser {
 public:
  PLpgSQLParser(common::ManagedPointer<UDFContext> udf_context, common::ManagedPointer<parser::PostgresParser> sql_parser)
  : udf_context_(udf_context), sql_parser_(sql_parser) {};
  std::unique_ptr<FunctionAST> ParsePLpgSQL(std::string func_body);

 private:
  std::unique_ptr<StmtAST> ParseBlock(const nlohmann::json &block);
  std::unique_ptr<StmtAST> ParseFunction(const nlohmann::json &block);
  std::unique_ptr<StmtAST> ParseDecl(const nlohmann::json &decl);
  std::unique_ptr<StmtAST> ParseIf(const nlohmann::json &branch);
  std::unique_ptr<StmtAST> ParseWhile(const nlohmann::json &loop);
  std::unique_ptr<StmtAST> ParseSQL(const nlohmann::json &sql_stmt);
  std::unique_ptr<StmtAST> ParseDynamicSQL(const nlohmann::json &sql_stmt);
  // Feed the expression (as a sql string) to our parser then transform the
  // terrier expression into ast node
  std::unique_ptr<ExprAST> ParseExprSQL(const std::string expr_sql_str);
  std::unique_ptr<ExprAST> ParseExpr(common::ManagedPointer<parser::AbstractExpression>);

  common::ManagedPointer<UDFContext> udf_context_;
  common::ManagedPointer<parser::PostgresParser> sql_parser_;
};
}  // namespace terrier::parser::udf