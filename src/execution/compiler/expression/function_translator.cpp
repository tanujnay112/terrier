#include "execution/compiler/expression/function_translator.h"
#include "execution/compiler/translator_factory.h"
#include "execution/sql/value.h"
#include "execution/udf/udf_context.h"

#include "parser/expression/function_expression.h"
#include "type/transient_value_peeker.h"

namespace terrier::execution::compiler {
FunctionTranslator::FunctionTranslator(const terrier::parser::AbstractExpression *expression, CodeGen *codegen)
    : ExpressionTranslator(expression, codegen) {
  for (auto child : expression->GetChildren()) {
    params_.push_back(TranslatorFactory::CreateExpressionTranslator(child.Get(), codegen_));
  }
  auto proc_oid = GetExpressionAs<parser::FunctionExpression>()->GetProcOid();
  udf_context_ = codegen_->Accessor()->GetUDFContext(proc_oid);
}

void FunctionTranslator::InitTopLevelDecls(util::RegionVector<ast::Decl *> *decls) {
  auto udf_decls = udf_context_->GetFile()->Declarations();
  for(ast::Decl *udf_decl : udf_decls){
    decls->emplace_back(udf_decl);
  }
}

ast::Expr *FunctionTranslator::DeriveExpr(ExpressionEvaluator *evaluator) {
//  if (!udf_ctx->IsBuiltin()) {
//    UNREACHABLE("We don't support non-builtin UDF's yet!");
//  }

  auto func_expr = GetExpressionAs<parser::FunctionExpression>();
  auto proc_oid = func_expr->GetProcOid();
  auto udf_ctx = codegen_->Accessor()->GetUDFContext(proc_oid);

  std::vector<ast::Expr *> params;
  if (udf_ctx->IsExecCtxRequired()) {
    params.push_back(codegen_->MakeExpr(codegen_->GetExecCtxVar()));
  }
  for (auto &param : params_) {
    params.push_back(param->DeriveExpr(evaluator));
  }

  if (!udf_context_->IsBuiltin()) {
//    UNREACHABLE("We don't support non-builtin UDF's yet!");
    codegen_->ExecCall(ast::Identifier(udf_context_->GetFunctionName().c_str()));
  }

  return codegen_->BuiltinCall(udf_context_->GetBuiltin(), std::move(params));
}
};  // namespace terrier::execution::compiler
