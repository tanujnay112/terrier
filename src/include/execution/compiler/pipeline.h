#pragma once

#include <memory>
#include <utility>
#include <vector>
#include "common/strong_typedef.h"
#include "execution/compiler/codegen.h"
#include "execution/compiler/function_builder.h"
#include "execution/compiler/operator/operator_translator.h"
#include "execution/exec_defs.h"
#include "execution/util/region.h"

namespace terrier::execution::compiler {

/**
 * A single pipeline
 */
class Pipeline {
 public:
  /**
   * Constructor
   * @param codegen the code generator to use
   */
  explicit Pipeline(CodeGen *codegen) : codegen_(codegen) {}

  /**
   * Add an operator translator to the pipeline
   * @param translator translator to add
   */
  void Add(std::unique_ptr<OperatorTranslator> &&translator) {
    is_vectorizable_ = is_vectorizable_ && translator->IsVectorizable();
    is_parallelizable_ = is_parallelizable_ && translator->IsParallelizable();
    pipeline_.emplace_back(std::move(translator));
  }

  /**
   * @return the pipelines' function id
   */
  ast::Identifier GetPipelineName() {
    return codegen_->Context()->GetIdentifier("pipeline" + std::to_string(!pipeline_idx_));
  }

  /**
   * Generate the top level declarations of this pipeline
   * @param decls list of functions and structs
   * @param state_fields list of state fields
   * @param setup_stmts list of stmts for the setup function
   * @param teardown_stmts list of stmts for the teardown functiono
   */
  void Initialize(util::RegionVector<ast::Decl *> *decls, util::RegionVector<ast::FieldDecl *> *state_fields,
                  util::RegionVector<ast::Stmt *> *setup_stmts, util::RegionVector<ast::Stmt *> *teardown_stmts);

  /**
   * Produce the code of this pipeline
   * @param query_id query identifier
   * @param pipeline_idx index of of this pipeline
   * @return the function generated by this pipeline
   */
  std::vector<ast::Decl *> Produce(query_id_t query_id, pipeline_id_t pipeline_idx);

  /**
   * Gets the vector of operators that make up the pipeline
   * @return vector of pipeline operators
   */
  const std::vector<std::unique_ptr<OperatorTranslator>> &GetTranslators() const { return pipeline_; }

 private:
  CodeGen *codegen_;
  std::vector<std::unique_ptr<OperatorTranslator>> pipeline_{};
  pipeline_id_t pipeline_idx_{0};
  bool is_vectorizable_{true};
  bool is_parallelizable_{true};
};

}  // namespace terrier::execution::compiler
