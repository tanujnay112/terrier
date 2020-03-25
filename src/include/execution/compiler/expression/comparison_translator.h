#pragma once
#include <memory>
#include "execution/compiler/expression/expression_translator.h"

namespace terrier::execution::compiler {

/**
 * Comparison Translator
 */
class ComparisonTranslator : public ExpressionTranslator {
 public:
  /**
   * Constructor
   * @param expression expression to translate
   * @param codegen code generator to use
   */
  ComparisonTranslator(const terrier::parser::AbstractExpression *expression, CodeGen *codegen);

  /**
   * Add top-level declarations
   * @param decls list of top-level declarations
   */
  void InitTopLevelDecls(util::RegionVector<ast::Decl *> *decls) override;

  ast::Expr *DeriveExpr(ExpressionEvaluator *evaluator) override;

 private:
  std::unique_ptr<ExpressionTranslator> left_;
  std::unique_ptr<ExpressionTranslator> right_;
};
}  // namespace terrier::execution::compiler
