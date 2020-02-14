#pragma once

#include <memory>
#include <string>
#include <vector>

#include "type/type_id.h"

namespace terrier::udf {

using arg_type = type::TypeId;

// ExprAST - Base class for all expression nodes.
class ExprAST {
 public:
  virtual ~ExprAST() = default;

};

// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  int val_;

 public:
  NumberExprAST(int val) : val_(val) {}

};

// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string name_;

 public:
  VariableExprAST(const std::string &name) : name_(name) {}

};

// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
  char op_;
  std::unique_ptr<ExprAST> lhs_, rhs_;

 public:
  BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs,
                std::unique_ptr<ExprAST> rhs)
      : op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

};

// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string callee_;
  std::vector<std::unique_ptr<ExprAST>> args_;
  std::string current_func_;
  std::vector<arg_type> args_type_;

 public:
  CallExprAST(const std::string &callee,
              std::vector<std::unique_ptr<ExprAST>> args,
              std::string &current_func, std::vector<arg_type> args_type)
      : callee_(callee), args_(std::move(args)) {
    current_func_ = current_func;
    args_type_ = args_type;
  }
};

/// IfExprAST - Expression class for if/then/else.
class IfExprAST : public ExprAST {
  std::unique_ptr<ExprAST> cond_expr_, then_stmt_, else_stmt_;

 public:
  IfExprAST(std::unique_ptr<ExprAST> cond_expr,
            std::unique_ptr<ExprAST> then_stmt,
            std::unique_ptr<ExprAST> else_stmt)
      : cond_expr_(std::move(cond_expr)),
        then_stmt_(std::move(then_stmt)),
        else_stmt_(std::move(else_stmt)) {}

};

// FunctionAST - This class represents a function definition itself.
class FunctionAST {
  std::unique_ptr<ExprAST> body_;

 public:
  FunctionAST(std::unique_ptr<ExprAST> body) : body_(std::move(body)) {}

};

/*----------------------------------------------------------------
/// Error* - These are little helper functions for error handling.
-----------------------------------------------------------------*/

std::unique_ptr<ExprAST> LogError(const char *str);

}  // namespace terrier::udf
