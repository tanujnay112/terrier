#pragma once

#include <vector>
#include "execution/ast/ast.h"
#include "execution/ast/ast_node_factory.h"
#include "execution/compiler/compiler_defs.h"
#include "execution/util/region_containers.h"

namespace tpl::compiler {

/* Basic unit of compilation. */
class CodeBlock {
 public:
  CodeBlock() : true_start_(this) {}

  void ChangeTrueStart(CodeBlock *start) { true_start_ = start; }

  void Append(ast::Stmt *stmt) { code_.emplace_back(stmt); }

  void Append(CodeBlock *stmt) { code_.emplace_back(stmt); }

  CodeBlock *NestedBlock() {
    auto child = new CodeBlock();
    code_.emplace_back(child);
    return child;
  }

  ast::BlockStmt *Compile(ast::AstNodeFactory *factory, util::Region *region) {
    util::RegionVector<ast::Stmt *> stmts(region);
    for (const auto &src : code_) {
      stmts.push_back(src.Compile(factory, region));
    }
    return factory->NewBlockStmt(DUMMY_POS, DUMMY_POS, std::move(stmts));
  }

  void Clear() {
    true_start_ = this;
    code_.clear();
  }

 private:
  /* Statements might come from other codeblocks. */
  class CodeWrapper {
   public:
    enum class SourceType : u8 { STMT, BLOCK };

    explicit CodeWrapper(ast::Stmt *stmt) : type_(SourceType::STMT), source_(stmt) {}
    explicit CodeWrapper(CodeBlock *block) : type_(SourceType::BLOCK), source_(block) {}

    ast::Stmt *Compile(ast::AstNodeFactory *factory, util::Region *region) const {
      switch (type_) {
        case SourceType::STMT:
          return reinterpret_cast<ast::Stmt *>(source_);
        case SourceType::BLOCK:
          return reinterpret_cast<CodeBlock *>(source_)->true_start_->Compile(factory, region);
      }
    }

    ~CodeWrapper() {
      if (type_ == SourceType::BLOCK) {
        delete reinterpret_cast<CodeBlock*>(source_);
      }
    }

   private:
    SourceType type_;
    void *source_;
  };

  std::vector<CodeWrapper> code_;
  CodeBlock *true_start_;
};

}  // namespace tpl::compiler