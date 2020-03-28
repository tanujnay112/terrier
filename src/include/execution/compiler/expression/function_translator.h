#pragma once

#include <memory>
#include <vector>

#include "execution/compiler/expression/expression_translator.h"

namespace terrier::execution::compiler {

/**
 * Function Translator
 */
class FunctionTranslator : public ExpressionTranslator {
 public:
  /**
   * Constructor
   * @param expression expression to translate
   * @param codegen code generator to use
   */
  FunctionTranslator(const terrier::parser::AbstractExpression *expression, CodeGen *codegen);

  /**
   * Add top-level declarations
   * @param decls list of top-level declarations
   */
  void InitTopLevelDecls(util::RegionVector<ast::Decl *> *decls) override;

  ast::Expr *DeriveExpr(ExpressionEvaluator *evaluator) override;

 private:
  std::vector<std::unique_ptr<ExpressionTranslator>> params_;
  common::ManagedPointer<udf::UDFContext> udf_context_;
};
}  // namespace terrier::execution::compiler
