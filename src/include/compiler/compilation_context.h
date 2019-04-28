#pragma once

#include <string>
#include "llvm/ADT/DenseMap.h"

#include "ast/identifier.h"
#include "ast/ast_node_factory.h"
#include "compiler/code_block.h"
#include "compiler/compiler_defs.h"
#include "planner/plannodes/abstract_plan_node.h"
#include "util/region.h"

namespace tpl::compiler {

class CompilationContext {
 public:
  CompilationContext() : region_("CompilationContext"), factory_(&region_), uniq_id_(0) {}

  // TODO(WAN): CRTP on plan nodes is good idea in hindsight..
  CodeBlock *Compile(CodeBlock *code_block, terrier::planner::AbstractPlanNode *plan_node) {
    switch (plan_node->GetPlanNodeType()) {
      case terrier::planner::PlanNodeType::AGGREGATE: {
//        const auto &node = As<terrier::planner::AggregatePlanNode>(plan_node);
//        const auto &agg_terms = node->GetAggregateTerms();

        // var counter = 0
        auto counter_id = NewIdentifier();
        auto counter_decl = factory_.NewVariableDecl(DUMMY_POS, counter_id, nullptr, factory_.NewIntLiteral(DUMMY_POS, 0));
        code_block->Append(factory_.NewDeclStmt(counter_decl));

        auto counter_id_expr = factory_.NewIdentifierExpr(DUMMY_POS, counter_id);
        // counter = counter + 1
        auto plus_one = factory_.NewBinaryOpExpr(DUMMY_POS, parsing::Token::Type::PLUS, counter_id_expr, factory_.NewIntLiteral(DUMMY_POS, 1));
        auto new_block = code_block->NestedBlock();
        new_block->Append(factory_.NewAssignmentStmt(DUMMY_POS, counter_id_expr, plus_one));

        // return counter
        code_block->Append(factory_.NewReturnStmt(DUMMY_POS, factory_.NewIdentifierExpr(DUMMY_POS, counter_id)));

        // all further modifications are to the nested block
        return new_block;
      }

      case terrier::planner::PlanNodeType::SEQSCAN: {
        auto *outer = new CodeBlock();
        auto target = factory_.NewIdentifierExpr(DUMMY_POS, NewIdentifier());
        auto table_name = factory_.NewIdentifierExpr(DUMMY_POS, ast::Identifier("table1"));
        auto inner_block = code_block->Compile(&factory_, &region_);
        auto for_in = factory_.NewForInStmt(DUMMY_POS, target, table_name, nullptr, inner_block);
        outer->Append(for_in);
        code_block->ChangeTrueStart(outer);
        return outer;
      }

      default: {
        return nullptr;
      }
    }
  }

  template <typename T>
  T* As(terrier::planner::AbstractPlanNode *plan_node) {
    return reinterpret_cast<T *>(plan_node);
  }

  ast::Identifier NewIdentifier() {
    auto *s = new std::string(std::to_string(uniq_id_++));
    return ast::Identifier(s->c_str());
  }

  ast::AstNodeFactory *GetFactory() { return &factory_; }
  util::Region *GetRegion() { return &region_; }

 private:
  llvm::DenseMap<ast::Identifier, ast::Identifier> table_iter_naming_;
  util::Region region_;
  ast::AstNodeFactory factory_;
  u64 uniq_id_;
};

}  // namespace tpl::compiler