#include "execution/ast/ast_clone.h"

#include <string>
#include <utility>

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"

#include "execution/ast/ast.h"
#include "execution/ast/ast_visitor.h"
#include "execution/ast/context.h"
#include "execution/ast/type.h"


namespace terrier::execution::ast {
using namespace terrier::execution::compiler;

 class AstCloneImpl : public AstVisitor<AstCloneImpl, AstNode*> {
 public:
  explicit AstCloneImpl(AstNode *root, AstNodeFactory *factory, Context *old_context,
      Context *new_context, std::string prefix)
      : root_(root), factory_{factory}, old_context_{old_context}, new_context_{new_context}, prefix_{prefix} {}

  AstNode *Run() { return Visit(root_); }

  // Declare all node visit methods here
#define DECLARE_VISIT_METHOD(type) AstNode *Visit##type(type *node);
  AST_NODES(DECLARE_VISIT_METHOD)
#undef DECLARE_VISIT_METHOD

  Identifier CloneIdentifier(Identifier &ident){
    return Identifier(ident);
  }

  Identifier CloneIdentifier(Identifier &&ident){
    auto type = old_context_->LookupBuiltinType(ident);
    if(type != nullptr){
      auto *builtin = type->SafeAs<BuiltinType>();
      TERRIER_ASSERT(builtin != nullptr, "There shouldn't be a non-builtin returned from LookupBuiltinType");
      return new_context_->GetBuiltinType(builtin->GetKind());
    }

    Builtin builtin_func;
    auto is_builtin_func = old_context_->IsBuiltinFunction(ident, &builtin_func);
    if(is_builtin_func){
      return new_context_->GetBuiltinFunction(builtin_func);
    }

    auto iter = allocated_strings_.find(ident.Data());
    llvm::StringRef value;
    if(iter == allocated_strings_.end()){
      auto *str = factory_->Region()->AllocateArray<char>(ident.Length() + 1);
      std::memcpy(str, ident.Data(), ident.Length() + 1);
      value = llvm::StringRef(str);
      allocated_strings_[ident.Data()] = value;
    }else{
      value = iter->second;
    }

    return new_context_->GetIdentifier(value);
  }

 private:
  AstNode *root_;
  AstNodeFactory *factory_;

  Context *old_context_;
  Context *new_context_;
  std::string prefix_;

  llvm::DenseMap<llvm::StringRef, llvm::StringRef> allocated_strings_;
};

AstNode *AstCloneImpl::VisitFile(File *node) {
  util::RegionVector<Decl *> decls(factory_->Region());
  for(auto *decl : node->Declarations()){
    decls.push_back(reinterpret_cast<Decl *>(Visit(decl)));
  }
  return factory_->NewFile(node->Position(), std::move(decls));
}

AstNode *AstCloneImpl::VisitFieldDecl(FieldDecl *node) {
  return factory_->NewFieldDecl(node->Position(), CloneIdentifier(node->Name()),
      reinterpret_cast<Expr*>(Visit(node->TypeRepr())));
}

AstNode *AstCloneImpl::VisitFunctionDecl(FunctionDecl *node) {
  return factory_->NewFunctionDecl(node->Position(), CloneIdentifier(node->Name()),
                            reinterpret_cast<FunctionLitExpr*>(VisitFunctionLitExpr(node->Function())));
}

AstNode *AstCloneImpl::VisitVariableDecl(VariableDecl *node) {
  return factory_->NewVariableDecl(node->Position(), CloneIdentifier(node->Name()),
      reinterpret_cast<Expr*>(node->TypeRepr()), node->Initial());
}

AstNode *AstCloneImpl::VisitStructDecl(StructDecl *node) {
  return factory_->NewStructDecl(node->Position(), CloneIdentifier(node->Name()),
                                 reinterpret_cast<StructTypeRepr*>(VisitStructTypeRepr(
                                     reinterpret_cast<StructTypeRepr*>(node->TypeRepr()))));
}

AstNode *AstCloneImpl::VisitAssignmentStmt(AssignmentStmt *node) {
  return factory_->NewAssignmentStmt(node->Position(),
      reinterpret_cast<Expr*>(Visit(node->Destination())), reinterpret_cast<Expr*>(Visit(node->Source())));
}

AstNode *AstCloneImpl::VisitBlockStmt(BlockStmt *node) {
  util::RegionVector<ast::Stmt*> stmts(factory_->Region());
  for (auto *stmt : node->Statements()) {
    stmts.push_back(reinterpret_cast<ast::Stmt*>(Visit(stmt)));
  }
  return factory_->NewBlockStmt(node->Position(), node->RightBracePosition(), std::move(stmts));
}

AstNode *AstCloneImpl::VisitDeclStmt(DeclStmt *node) {
  return factory_->NewDeclStmt(reinterpret_cast<Decl*>(VisitDecl(node->Declaration())));
}

AstNode *AstCloneImpl::VisitExpressionStmt(ExpressionStmt *node) {
  return factory_->NewExpressionStmt(reinterpret_cast<Expr*>(Visit(node->Expression())));
}

AstNode *AstCloneImpl::VisitForStmt(ForStmt *node) {
  return factory_->NewForStmt(node->Position(),
      reinterpret_cast<Stmt*>(VisitStmt(node->Init())), reinterpret_cast<Expr*>(Visit(node->Condition())),
                              reinterpret_cast<Stmt*>(Visit(node->Next())),
                              reinterpret_cast<BlockStmt*>(VisitBlockStmt(node->Body())));
}

AstNode *AstCloneImpl::VisitForInStmt(ForInStmt *node) {
  return factory_->NewForInStmt(node->Position(), reinterpret_cast<Expr*>(Visit(node->Target())),
                                reinterpret_cast<Expr*>(Visit(node->Iter())),
                                reinterpret_cast<BlockStmt*>(VisitBlockStmt(node->Body())));
}

AstNode *AstCloneImpl::VisitIfStmt(IfStmt *node) {
  auto *else_stmt = node->ElseStmt() == nullptr ? nullptr : reinterpret_cast<Stmt*>(Visit((node->ElseStmt())));
  return factory_->NewIfStmt(node->Position(),
                                reinterpret_cast<Expr*>(Visit(node->Condition())),
                                    reinterpret_cast<BlockStmt*>(VisitBlockStmt(node->ThenStmt())),
                                    else_stmt);
}

AstNode *AstCloneImpl::VisitReturnStmt(ReturnStmt *node) {
  return factory_->NewReturnStmt(node->Position(), reinterpret_cast<Expr*>(Visit(node->Ret())));
}

AstNode *AstCloneImpl::VisitCallExpr(CallExpr *node) {
  util::RegionVector<Expr*> args(factory_->Region());

  for(auto *arg : node->Arguments()){
    args.push_back(reinterpret_cast<Expr*>(Visit(arg)));
  }
  if(node->GetCallKind() == CallExpr::CallKind::Builtin){
    return factory_->NewBuiltinCallExpr(reinterpret_cast<Expr*>(Visit(node->Function())), std::move(args));
  }
  return factory_->NewCallExpr(reinterpret_cast<Expr*>(Visit(node->Function())), std::move(args));
}

AstNode *AstCloneImpl::VisitBinaryOpExpr(BinaryOpExpr *node) {
  return factory_->NewBinaryOpExpr(node->Position(), node->Op(),
      reinterpret_cast<Expr*>(Visit(node->Left())),
                                   reinterpret_cast<Expr*>(Visit(node->Right())));
}

AstNode *AstCloneImpl::VisitComparisonOpExpr(ComparisonOpExpr *node) {
  return factory_->NewComparisonOpExpr(node->Position(), node->Op(),
                                   reinterpret_cast<Expr*>(Visit(node->Left())),
                                   reinterpret_cast<Expr*>(Visit(node->Right())));
}

AstNode *AstCloneImpl::VisitFunctionLitExpr(FunctionLitExpr *node) {
  return factory_->NewFunctionLitExpr(reinterpret_cast<FunctionTypeRepr*>(VisitFunctionTypeRepr(node->TypeRepr())),
                                      reinterpret_cast<BlockStmt*>(VisitBlockStmt(node->Body())));
}

AstNode *AstCloneImpl::VisitIdentifierExpr(IdentifierExpr *node) {
  return factory_->NewIdentifierExpr(node->Position(), CloneIdentifier(node->Name()));
}

AstNode *AstCloneImpl::VisitImplicitCastExpr(ImplicitCastExpr *node) {
  // TODO(tanujnay112) The type might have to be cloned
  return Visit(node->Input());
}

AstNode *AstCloneImpl::VisitIndexExpr(IndexExpr *node) {
  return factory_->NewIndexExpr(node->Position(), reinterpret_cast<Expr*>(Visit(node->Object())),
                                reinterpret_cast<Expr*>(Visit(node->Index())));
}

AstNode *AstCloneImpl::VisitLitExpr(LitExpr *node) {
  AstNode *literal;
  switch (node->LiteralKind()) {
    case LitExpr::LitKind::Nil: {
      literal = factory_->NewNilLiteral(node->Position());
      break;
    }
    case LitExpr::LitKind::Boolean: {
      literal = factory_->NewBoolLiteral(node->Position(), node->BoolVal());
      break;
    }
    case LitExpr::LitKind::Int: {
      literal = factory_->NewIntLiteral(node->Position(), node->Int64Val());
      break;
    }
    case LitExpr::LitKind::Float: {
      literal = factory_->NewFloatLiteral(node->Position(), node->Float64Val());
      break;
    }
    case LitExpr::LitKind::String: {
      literal = factory_->NewStringLiteral(node->Position(), CloneIdentifier(node->RawStringVal()));
      break;
    }
  }
  TERRIER_ASSERT(literal != nullptr, "Unknown literal kind");
  return literal;
}

AstNode *AstCloneImpl::VisitMemberExpr(MemberExpr *node) {
  return factory_->NewIndexExpr(node->Position(), reinterpret_cast<Expr*>(Visit(node->Object())),
                                reinterpret_cast<Expr*>(Visit(node->Member())));
}

AstNode *AstCloneImpl::VisitUnaryOpExpr(UnaryOpExpr *node) {
  return factory_->NewUnaryOpExpr(node->Position(), node->Op(),
                                   reinterpret_cast<Expr*>(Visit(node->Expression())));
}

AstNode *AstCloneImpl::VisitBadExpr(BadExpr *node) {
  return factory_->NewBadExpr(node->Position());
}

AstNode *AstCloneImpl::VisitStructTypeRepr(StructTypeRepr *node) {
  util::RegionVector<FieldDecl *> field_decls(factory_->Region());
  return factory_->NewStructType(node->Position(), std::move(field_decls));
}

AstNode *AstCloneImpl::VisitPointerTypeRepr(PointerTypeRepr *node) {
  return factory_->NewPointerType(node->Position(), reinterpret_cast<Expr*>(Visit(node->Base())));
}

AstNode *AstCloneImpl::VisitFunctionTypeRepr(FunctionTypeRepr *node) {
  util::RegionVector<FieldDecl*> params(factory_->Region());
  for(auto *param : node->Paramaters()){
    params.push_back(reinterpret_cast<FieldDecl*>(VisitFieldDecl(param)));
  }

  return factory_->NewFunctionType(node->Position(), std::move(params),
                                   reinterpret_cast<Expr*>(Visit(node->ReturnType())));
}

AstNode *AstCloneImpl::VisitArrayTypeRepr(ArrayTypeRepr *node) {
  return factory_->NewArrayType(node->Position(),reinterpret_cast<Expr*>(Visit(node->Length())),
                                  reinterpret_cast<Expr*>(Visit(node->ElementType())));
}

AstNode *AstCloneImpl::VisitMapTypeRepr(MapTypeRepr *node) {
  return factory_->NewMapType(node->Position(), reinterpret_cast<Expr*>(Visit(node->Key())),
                              reinterpret_cast<Expr*>(Visit(node->Val())));
}

AstNode *AstClone::Clone(AstNode *node, AstNodeFactory *factory, std::string prefix,
    Context *old_context, Context *new_context) {
  AstCloneImpl cloner(node, factory, old_context, new_context, prefix);
  return cloner.Run();
}

}  // namespace terrier::execution::ast
