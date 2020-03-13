#pragma once

#include "type/type_id.h"

namespace terrier::parser::udf {
class UDFASTContext {
 public:
  UDFASTContext() {}

  void SetVariableType(std::string &var, type::TypeId type) {
    symbol_table_[var] = type;
  }

  type::TypeId GetVariableType(std::string &var) { return symbol_table_[var]; }

  void AddVariable(std::string name) { local_variables_.push_back(name); }

  std::string &GetVariableAtIndex(int index) { return local_variables_[index]; }

 private:
  std::unordered_map<std::string, type::TypeId> symbol_table_;
  std::vector<std::string> local_variables_;
};

}  // namespace terrier::parser::udf
