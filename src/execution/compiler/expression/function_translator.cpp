#include "execution/compiler/expression/function_translator.h"
#include "execution/compiler/function_builder.h"
#include "execution/compiler/translator_factory.h"
#include "execution/sql/value.h"
#include "execution/udf/udf_context.h"

#include "parser/expression/function_expression.h"
#include "parser/udf/udf_codegen.h"
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
//  auto udf_decls = udf_context_->GetFile()->Declarations();
//  for(ast::Decl *udf_decl : udf_decls){
//    decls->emplace_back(udf_decl);
//  }
    if(udf_context_->IsBuiltin()){
      return;
    }

    util::RegionVector<ast::FieldDecl *> fn_params{codegen_->Region()};
//  for(size_t i = 0;i < udf_context_->GetFunctionArgsType().size();i++) {
//    auto name = node->GetFunctionParameterNames()[i];
//    auto type = parser::ReturnType::DataTypeToTypeId(node->GetFunctionParameterTypes()[i]);
//    auto name_alloc = reinterpret_cast<char*>(codegen.Region()->Allocate(name.length()+1));
//    std::memcpy(name_alloc, name.c_str(), name.length() + 1);
//    fn_params.emplace_back(codegen.MakeField(ast::Identifier{name_alloc}, codegen.TplType(type)));
//  }
//
//    auto name = udf_context_->GetFunctionName();
//    char *name_alloc = reinterpret_cast<char*>(codegen_->Region()->Allocate(name.length() + 1));
//    std::memcpy(name_alloc, name.c_str(), name.length() + 1);
//
    compiler::FunctionBuilder fb{codegen_, ast::Identifier{udf_context_->GetFunctionName().c_str()}, std::move(fn_params),
                                 codegen_->TplType(udf_context_->GetFunctionReturnType())};
  parser::udf::UDFCodegen udf_codegen{&fb, nullptr, codegen_};
  udf_codegen.GenerateUDF(udf_context_->GetASTHead()->body.get());
  auto fn = fb.Finish();
  decls->emplace_back(fn);
////  util::RegionVector<ast::Decl *> decls_reg_vec{decls->begin(), decls->end(), codegen.Region()};
//  util::RegionVector<ast::Decl*> decls({fn}, codegen.Region());
//  auto file = codegen.Compile(std::move(decls));
}

ast::Expr *FunctionTranslator::DeriveExpr(ExpressionEvaluator *evaluator) {
//  if (!udf_ctx->IsBuiltin()) {
//    UNREACHABLE("We don't support non-builtin UDF's yet!");
//  }

//  auto func_expr = GetExpressionAs<parser::FunctionExpression>();
//  auto proc_oid = func_expr->GetProcOid();
//  auto udf_ctx = codegen_->Accessor()->GetUDFContext(proc_oid);

  std::vector<ast::Expr *> params;
  if (udf_context_->IsBuiltin() && udf_context_->IsExecCtxRequired()) {
    params.push_back(codegen_->MakeExpr(codegen_->GetExecCtxVar()));
  }
  for (auto &param : params_) {
    params.push_back(param->DeriveExpr(evaluator));
  }

  if (!udf_context_->IsBuiltin()) {
//    UNREACHABLE("We don't support non-builtin UDF's yet!");
    return codegen_->ExecCallExpr(ast::Identifier(udf_context_->GetFunctionName().c_str()));
  }

  return codegen_->BuiltinCall(udf_context_->GetBuiltin(), std::move(params));
}
};  // namespace terrier::execution::compiler
