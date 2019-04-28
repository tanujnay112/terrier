#pragma once

#include "ast/ast.h"
#include "ast/context.h"
#include "ast/ast_dump.h"
#include "ast/ast_node_factory.h"
#include "ast/context.h"
#include "ast/type.h"
#include "compiler/compilation_context.h"
#include "planner/plannodes/abstract_plan_node.h"
#include "util/region.h"
#include "sema/sema.h"

namespace tpl::compiler {

/*
 * Input: plan nodes
 * Output: TPL DSL
 */
class PlanNodeCompiler {
 public:
  ast::AstNode *Compile(terrier::planner::AbstractPlanNode *plan_node) {
    // Setup our compilation context. We use the same one throughout the entire compilation process.
    CompilationContext ctx;
    CodeBlock block;
    CodeBlock *curr_block = &block;

    curr_block = ctx.Compile(curr_block, plan_node);

    for (const auto &child : plan_node->GetChildren()) {
      curr_block = ctx.Compile(curr_block, child.get());
    }

    auto ret = block.Compile(ctx.GetFactory(), ctx.GetRegion());

    util::RegionVector<ast::FieldDecl *> params(ctx.GetRegion());
    auto ret_type = ctx.GetFactory()->NewIdentifierExpr(DUMMY_POS, ast::Identifier("int32"));
    auto type = ctx.GetFactory()->NewFunctionType(DUMMY_POS, std::move(params), ret_type);
    auto lit = ctx.GetFactory()->NewFunctionLitExpr(type, ret);
    auto fn = ctx.GetFactory()->NewFunctionDecl(DUMMY_POS, ast::Identifier("moo"), lit);
    sema::ErrorReporter errors(ctx.GetRegion());
    ast::Context ast_ctx(ctx.GetRegion(), &errors);
    sema::Sema type_check(&ast_ctx);
    type_check.Run(fn);

    ast::AstDump::Dump(fn);
    return ret;
  }
 private:
};

}  // namespace tpl::compiler