#pragma once

#include <stdlib.h>

#include "type/type_util.h"

namespace terrier::parser::udf {

class UDFContext {
public:
  UDFContext(std::string &func_name, type::TypeId func_ret_type,
             std::vector<type::TypeId> &args_type) : func_name_(func_name),
             func_ret_type_(func_ret_type), args_type_(args_type) {}

  void SetFunctionName(std::string func_name) { func_name_ = func_name; }
  
  std::string GetFunctionName() { return func_name_; }

  const std::vector<type::TypeId> &GetFunctionArgsType() { return args_type_; }

  void SetFunctionReturnType(type::TypeId type) { func_ret_type_ = type; }
  
  type::TypeId GetFunctionReturnType() { return func_ret_type_; }

  void SetVariableType(std::string &var, type::TypeId type) {
    symbol_table_[var] = type;
  }

  type::TypeId GetVariableType(std::string &var) { return symbol_table_[var]; }

//  void SetAllocValue(std::string &var, peloton::codegen::Value val) {
//    named_values_[var] = val;
//  }
//
//  peloton::codegen::Value GetAllocValue(std::string &var) {
//    return named_values_[var];
//  }

  void AddVariable(std::string name) { local_variables_.push_back(name); }
  
  std::string GetVariableAtIndex(int index) { return local_variables_[index]; }

private:
  std::string func_name_;
  type::TypeId func_ret_type_;
  std::vector<type::TypeId> args_type_;
  std::unordered_map<std::string, type::TypeId> symbol_table_;
  std::vector<std::string> local_variables_;
};

}  // namespace terrier::udf
