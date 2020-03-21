#pragma once

#include "execution/compiler/function_builder.h"
#include "ast_node_visitor.h"
#include "udf_ast_context.h"
#include "execution/udf/udf_context.h"

namespace terrier::parser::udf {
using namespace execution::compiler;

class AbstractAST;
class StmtAST;
class ExprAST;
class ValueExprAST;
class VariableExprAST;
class BinaryExprAST;
class CallExprAST;
class SeqStmtAST;
class DeclStmtAST;
class IfStmtAST;
class WhileStmtAST;
class RetStmtAST;
class AssignStmtAST;
class SQLStmtAST;
class DynamicSQLStmtAST;

class UDFCodegen : ASTNodeVisitor {
 public:

  UDFCodegen(FunctionBuilder *fb,
             execution::udf::UDFContext *udf_context) : fb_{fb} {};
  ~UDFCodegen(){};

  void GenerateUDF(AbstractAST *);
  void Visit(AbstractAST *) override;
//  void Visit(FunctionAST *) override;
  void Visit(StmtAST *) override;
  void Visit(ExprAST *) override;
  void Visit(ValueExprAST *) override;
  void Visit(VariableExprAST *) override;
  void Visit(BinaryExprAST *) override;
  void Visit(CallExprAST *) override;
  void Visit(SeqStmtAST *) override;
  void Visit(DeclStmtAST *) override;
  void Visit(IfStmtAST *) override;
  void Visit(WhileStmtAST *) override;
  void Visit(RetStmtAST *) override;
  void Visit(AssignStmtAST *) override;
  void Visit(SQLStmtAST *) override;
  void Visit(DynamicSQLStmtAST *) override;

 private:
  FunctionBuilder *fb_;
  UDFASTContext *udf_ast_context_;
  CodeGen *codegen_;
  execution::ast::Expr *dst_;
  std::unordered_map<std::string, execution::ast::Identifier> str_to_ident_;
};
}  // namespace terrier::parser::udf
