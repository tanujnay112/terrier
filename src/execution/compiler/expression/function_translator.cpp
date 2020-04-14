#include "execution/compiler/expression/function_translator.h"
#include "execution/compiler/function_builder.h"
#include "execution/compiler/translator_factory.h"
#include "execution/ast/ast_clone.h"
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
    if(udf_context_->IsBuiltin()){
      return;
    }
    auto *file = reinterpret_cast<execution::ast::File*>(ast::AstClone::Clone(udf_context_->GetFile(),
        codegen_->Factory(), "", udf_context_->GetContext(), codegen_->Context()));
  auto udf_decls = file->Declarations();
//  main_fn_ = udf_decls.back()->Name();
  for(ast::Decl *udf_decl : udf_decls){
    decls->emplace_back(udf_decl);
  }
  udf_context_->SetStorage(udf_decls.back()->Name().Data());
//
////    auto *node = GetExpressionAs<parser::FunctionExpression>();
//    auto &param_names = udf_context_->GetASTHead()->param_names_;
//    auto &param_types = udf_context_->GetASTHead()->param_types_;
//    util::RegionVector<ast::FieldDecl *> fn_params{codegen_->Region()};
//  for(size_t i = 0;i < udf_context_->GetFunctionArgsType().size();i++) {
//      auto &param_name = param_names[i];
//      auto &param_type = param_types[i];
////    auto type = parser::ReturnType::DataTypeToTypeId(node->GetFunctionParameterTypes()[i]);
////    auto name_alloc = reinterpret_cast<char*>(codegen.Region()->Allocate(name.length()+1));
////    std::memcpy(name_alloc, name.c_str(), name.length() + 1);
//    fn_params.emplace_back(codegen_->MakeField(ast::Identifier{param_name.c_str()}, codegen_->TplType(param_type)));
//  }
////
////    auto name = udf_context_->GetFunctionName();
////    char *name_alloc = reinterpret_cast<char*>(codegen_->Region()->Allocate(name.length() + 1));
////    std::memcpy(name_alloc, name.c_str(), name.length() + 1);
//
//    compiler::FunctionBuilder fb{codegen_, ast::Identifier{udf_context_->GetFunctionName().c_str()}, std::move(fn_params),
//                                 codegen_->TplType(udf_context_->GetFunctionReturnType())};
//  parser::udf::UDFCodegen udf_codegen{&fb, nullptr, codegen_};
//  udf_codegen.GenerateUDF(udf_context_->GetASTHead().Get());
//  auto fn = fb.Finish();
//  decls->emplace_back(fn);
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
    auto ident_expr = codegen_->Factory()->NewIdentifierExpr(DUMMY_POS, ast::Identifier(udf_context_->GetStorage()));
    util::RegionVector<ast::Expr *> args{{}, codegen_->Region()};
    for (auto &expr : params) {
      args.emplace_back(expr);
    }
    return codegen_->Factory()->NewCallExpr(ident_expr, std::move(args));
  }

  return codegen_->BuiltinCall(udf_context_->GetBuiltin(), std::move(params));
}
};  // namespace terrier::execution::compiler
