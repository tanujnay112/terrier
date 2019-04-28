//#pragma once
//
//#include "ast/ast.h"
//#include "ast/ast_node_factory.h"
//#include "compiler/compiler_defs.h"
//
//
//namespace tpl::compiler {
//
//class Function;
//
//class Value {
// public:
//  explicit Value(ast::Expr *t, std::string &&name) : name_(name), type_(t) {}
//
//  virtual ast::Identifier GetIdentifier() const {
//    return ast::Identifier(name_.data());
//  }
//
//  virtual ast::Expr *GetIdentifierExpr(ast::AstNodeFactory *factory) const {
//    return factory->NewIdentifierExpr(DUMMY_POS, GetIdentifier());
//  }
//
//  std::string &GetName() { return name_; };
//  ast::Expr *GetExpr() { return type_; }
//
// private:
//  friend class Function;
//  std::string name_;
//  ast::Expr *type_;
//};
//
//template <class T>
//class Constant : public Value {
// public:
//  explicit Constant(ast::Expr *t, std::string &&name, T value)
//      : Value(t, std::move(name)), value_(value) {
//    TPL_ASSERT(GetExpr()->IsLitExpr(), "Not a literal type.");
//  }
//
//  ast::Identifier GetIdentifier() const override {
//    return ast::Identifier(value_.data());
//  }
//
////  ast::Expr *GetIdentifierExpr(ast::AstNodeFactory *factory) const override;
//
// private:
//  friend class Function;
//  T value_;
//};
//
//class StructMember : public Value {
// public:
//  explicit StructMember(ast::Expr *t, std::string &&name, ast::Identifier parent)
//      : Value(t, std::move(name)) {
//    hierarchy_.emplace_back(parent);
//  }
//
//  ast::Expr *GetIdentifierExpr(ast::AstNodeFactory *factory) const override {
//    return factory->NewMemberExpr(
//        DUMMY_POS, factory->NewIdentifierExpr(DUMMY_POS, hierarchy_.back()),
//        factory->NewIdentifierExpr(DUMMY_POS, GetIdentifier()));
//  }
//
// private:
//  std::vector<ast::Identifier> hierarchy_;
//};
//
//class StructVal : public Value {
// public:
//  explicit StructVal(ast::Expr *t, std::string &&name) : Value(t, std::move(name)) {}
//
//  StructMember *GetStructMember(std::string &&member_name) {
//    return new StructMember(GetExpr(), std::move(member_name), GetIdentifier());
//  }
//};
//
//class CodeBlock {
// public:
//  explicit CodeBlock(ast::AstNodeFactory *factory, util::Region *region)
//      : factory_(factory), blocks_(region) {}
//
//  ast::Stmt *AssignVariable(const Value *assignee, Value *val) {
//    auto ret = factory_->NewAssignmentStmt(
//        DUMMY_POS,
//        factory_->NewIdentifierExpr(DUMMY_POS, assignee->GetIdentifier()),
//        factory_->NewIdentifierExpr(DUMMY_POS, val->GetIdentifier()));
//    blocks_.push_back(ret);
//    return ret;
//  }
//
//  Value *DeclareVariable(ast::Expr *type, std::string &name) {
//    auto *variable = new Value(type, std::move(name));
//    auto stmt = factory_->NewDeclStmt(factory_->NewVariableDecl(
//        DUMMY_POS, variable->GetIdentifier(), type, nullptr));
//    blocks_.push_back(stmt);
//    return variable;
//  }
//
//  Value *InitializeVariable(ast::Expr *type, std::string &name, ast::Expr *init_val) {
//    auto *variable = new Value(type, std::move(name));
//    auto stmt = factory_->NewDeclStmt(factory_->NewVariableDecl(
//        DUMMY_POS, variable->GetIdentifier(), type, init_val));
//    blocks_.push_back(stmt);
//    return variable;
//  }
//
//  ast::Stmt *CreateForInLoop(Value *target, Value *iter, CodeBlock *body) {
//    auto *ret = factory_->NewForInStmt(
//          DUMMY_POS, target->GetExpr(), iter->GetExpr(), nullptr, body->Compile());
//    blocks_.push_back(ret);
//    return ret;
//  }
//
//  ast::BinaryOpExpr *ExecBinaryOp(Value *left, Value *right,
//                                  parsing::Token::Type op) {
//    return factory_->NewBinaryOpExpr(
//        DUMMY_POS, op, left->GetIdentifierExpr(factory_),
//        right->GetIdentifierExpr(factory_));
//  }
//
//  ast::UnaryOpExpr *ExecUnaryOp(Value *target, parsing::Token::Type op) {
//    return factory_->NewUnaryOpExpr(DUMMY_POS, op, target->GetIdentifierExpr(factory_));
//  }
//
//  ast::Stmt *CreateIfStmt(ast::Expr *cond, CodeBlock *then_block, CodeBlock *else_block) {
//    auto *ret = factory_->NewIfStmt(DUMMY_POS, cond, then_block->Compile(),
//        else_block == nullptr ? nullptr : else_block->Compile());
//    blocks_.emplace_back(ret);
//    return ret;
//  }
//
//  ast::Stmt *CreateForStmt(ast::Stmt *init, ast::Expr *cond, ast::Stmt *next, CodeBlock *body) {
//    auto *ret = factory_->NewForStmt(DUMMY_POS, init, cond, next, body->Compile());
//    blocks_.emplace_back(ret);
//    return ret;
//  }
//
//  ast::Stmt *BinaryOp(Value *dest, Value *left, Value *right, parsing::Token::Type op) {
//    auto bin_op = ExecBinaryOp(left, right, op);
//    auto *ret = factory_->NewAssignmentStmt(DUMMY_POS, dest->GetIdentifierExpr(factory_), bin_op);
//    blocks_.emplace_back(ret);
//    return ret;
//  }
//
//  ast::Stmt *OpAdd(Value *dest, Value *left, Value *right) {
//    return BinaryOp(dest, left, right, parsing::Token::Type::PLUS);
//  }
//
////  ast::Stmt *Call(Function *fn, std::initializer_list<Value *> arguments);
//
//  ast::BlockStmt *Compile() {
//    return factory_->NewBlockStmt(DUMMY_POS, DUMMY_POS, std::move(blocks_));
//  }
//
// private:
//  ast::AstNodeFactory *factory_;
//  util::RegionVector<ast::Stmt *> blocks_;
//};
//
//class Function {
// public:
//  explicit Function(ast::AstNodeFactory *factory, util::Region *region, std::string name,
//                    std::vector<Value *> &&params, ast::Expr *ret_type)
//      : region_(region), factory_(factory), name_(std::move(name)), ret_type_(ret_type), params_(std::move(params)), body_(factory_, region) {
//  }
//
//  ast::Identifier GetIdentifier() {
//    return ast::Identifier(name_.data());
//  }
//
//  ast::IdentifierExpr *GetIdentifierExpr() {
//    return factory_->NewIdentifierExpr(DUMMY_POS, GetIdentifier());
//  }
//
//  std::string GetName() { return name_; }
//
//  CodeBlock *GetCodeBlock() { return &body_; };
//
//  void Compile() {
//    ast::Identifier identifier(name_.data());
//    util::RegionVector<ast::FieldDecl *> fields(region_);
//    fields.reserve(4);  // TODO(WAN): why 4? @tanuj
//
//    for (const auto &v : params_) {
//      fields.push_back(factory_->NewFieldDecl(DUMMY_POS, ast::Identifier(v->name_.data()), v->GetExpr()));
//    }
//
//    ast::FunctionTypeRepr *fn_type =
//        factory_->NewFunctionType(DUMMY_POS, std::move(fields), ret_type_);
//    ast::Identifier fn_name(name_.data());
//    fn_decl_ = factory_->NewFunctionDecl(
//        DUMMY_POS, fn_name, factory_->NewFunctionLitExpr(fn_type, body_.Compile()));
//  }
//
// private:
//  util::Region *region_;
//  ast::AstNodeFactory *factory_;
//  std::string name_;
//  ast::Expr *ret_type_;
//  std::vector<Value *> params_;
//  ast::FunctionDecl *fn_decl_;
//  CodeBlock body_;
//};
//
//}  // namespace tpl::compiler
