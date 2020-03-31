#pragma once

#include <string>
#include <utility>
#include <vector>

#include "catalog/catalog_defs.h"
#include "common/managed_pointer.h"
#include "execution/ast/ast.h"
#include "execution/ast/builtins.h"
#include "parser/udf/ast_nodes.h"
#include "type/type_id.h"

namespace terrier::execution::udf {

/**
 * @brief Stores execution and type information about a stored procedure
 */
class UDFContext {
 public:
  /**
   * Creates a non-builtin UDFContext object
   * @param func_name Name of function
   * @param func_ret_type Return type of function
   * @param args_type Vector of argument types
   */
  UDFContext(std::string func_name, type::TypeId func_ret_type, std::vector<type::TypeId> &&args_type,
             std::unique_ptr<parser::udf::FunctionAST> &&head)
      : func_name_(std::move(func_name)),
        func_ret_type_(func_ret_type),
        args_type_(std::move(args_type)),
        is_builtin_{false},
        is_exec_ctx_required_{false},
        head_{std::move(head)}{}
  /**
   * Creates a UDFContext object for a builtin function
   * @param func_name Name of function
   * @param func_ret_type Return type of function
   * @param args_type Vector of argument types
   * @param builtin Which builtin this context refers to
   * @param is_exec_ctx_required true if this function requires an execution context var as its first argument
   */
  UDFContext(std::string func_name, type::TypeId func_ret_type, std::vector<type::TypeId> &&args_type,
             ast::Builtin builtin, bool is_exec_ctx_required = false)
      : func_name_(std::move(func_name)),
        func_ret_type_(func_ret_type),
        args_type_(std::move(args_type)),
        is_builtin_{true},
        builtin_{builtin},
        is_exec_ctx_required_{is_exec_ctx_required} {}
//        ast_region_{nullptr} {}
  /**
   * @return The name of the function represented by this context object
   */
  const std::string &GetFunctionName() const { return func_name_; }

  /**
   * @return The vector of type arguments of the function represented by this context object
   */
  const std::vector<type::TypeId> &GetFunctionArgsType() const { return args_type_; }

  /**
   * Gets the return type of the function represented by this object
   * @return return type of this function
   */
  type::TypeId GetFunctionReturnType() const { return func_ret_type_; }

  /**
   * @return true iff this represents a builtin function
   */
  bool IsBuiltin() const { return is_builtin_; }

  /**
   * @return returns what builtin function this represents
   */
  ast::Builtin GetBuiltin() const {
    TERRIER_ASSERT(IsBuiltin(), "Getting a builtin from a non-builtin function");
    return builtin_;
  }

  /**
   * @return returns if this function requires an execution context
   */
  bool IsExecCtxRequired() const {
    TERRIER_ASSERT(IsBuiltin(), "IsExecCtxRequired is only valid or a builtin function");
    return is_exec_ctx_required_;
  }

  common::ManagedPointer<parser::udf::FunctionAST> GetASTHead() const {
    TERRIER_ASSERT(!IsBuiltin(), "Not a builtin function");
    return common::ManagedPointer(head_);
  }

//  /**
//   * @return returns the main functiondecl of this udf (to be used only if not builtin)
//   */
//  common::ManagedPointer<ast::FunctionDecl> GetMainFunctionDecl() const {
//    TERRIER_ASSERT(!IsBuiltin(), "Getting a non-builtin from a builtin function");
//    return common::ManagedPointer<ast::FunctionDecl>(
//        reinterpret_cast<ast::FunctionDecl*>(file_->Declarations().back()));
//  }
//
//  /**
//   * @return returns the file with the functiondecl and supporting decls (to be used only if not builtin)
//   */
//  common::ManagedPointer<ast::File> GetFile() const {
//    TERRIER_ASSERT(!IsBuiltin(), "Getting a non-builtin from a builtin function");
//    return file_;
//  }
//
//  /**
//   * @return region used for allocation
//   */
//  util::Region *Region() { return ast_region_.get(); }

 private:
  std::string func_name_;
  type::TypeId func_ret_type_;
  std::vector<type::TypeId> args_type_;

  bool is_builtin_;
  ast::Builtin builtin_;
  bool is_exec_ctx_required_;

  // use only if is_builtin_ is false
//  common::ManagedPointer<ast::File> file_;
//  std::unique_ptr<util::Region> ast_region_;
  std::unique_ptr<parser::udf::FunctionAST> head_;
};

}  // namespace terrier::execution::udf
