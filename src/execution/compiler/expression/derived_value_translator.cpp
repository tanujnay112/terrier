#include "execution/compiler/expression/derived_value_translator.h"
#include "execution/compiler/operator/seq_scan_translator.h"
#include "execution/compiler/translator_factory.h"
#include "parser/expression/derived_value_expression.h"

namespace terrier::execution::compiler {
DerivedValueTranslator::DerivedValueTranslator(const terrier::parser::AbstractExpression *expression, CodeGen *codegen)
    : ExpressionTranslator(expression, codegen) {}

void DerivedValueTranslator::InitTopLevelDecls(util::RegionVector<ast::Decl *> *decls) {}

ast::Expr *DerivedValueTranslator::DeriveExpr(ExpressionEvaluator *evaluator) {
  auto derived_val = GetExpressionAs<terrier::parser::DerivedValueExpression>();
  return evaluator->GetChildOutput(derived_val->GetTupleIdx(), derived_val->GetValueIdx(),
                                   derived_val->GetReturnValueType());
}
};  // namespace terrier::execution::compiler
