#pragma once
#include <memory>
#include "execution/compiler/expression/expression_translator.h"

namespace terrier::execution::compiler {

/**
 * Parameter Value Translator
 */
class ParamValueTranslator : public ExpressionTranslator {
 public:
  /**
   * Constructor
   * @param expression expression to translate
   * @param codegen code generator to use
   */
  ParamValueTranslator(const terrier::parser::AbstractExpression *expression, CodeGen *codegen);

  /**
   * Add top-level declarations
   * @param decls list of top-level declarations
   */
  void InitTopLevelDecls(util::RegionVector<ast::Decl *> *decls) override;

  ast::Expr *DeriveExpr(ExpressionEvaluator *evaluator) override;
};
}  // namespace terrier::execution::compiler
