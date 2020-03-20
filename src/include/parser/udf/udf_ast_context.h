#pragma once

#include "type/type_id.h"
#include "common/exception.h"

namespace terrier::parser::udf {
class UDFASTContext {
 public:
  UDFASTContext() {}

  void SetVariableType(std::string &var, type::TypeId type) {
    symbol_table_[var] = type;
  }

  type::TypeId GetVariableType(const std::string &var) {
    auto it = symbol_table_.find(var);
    if(it == symbol_table_.end()){
      throw PARSER_EXCEPTION("Undeclared variable");
    }
    return it->second;
  }

  void AddVariable(std::string name) { local_variables_.push_back(name); }

  std::string &GetVariableAtIndex(int index) { return local_variables_[index]; }

 private:
  std::unordered_map<std::string, type::TypeId> symbol_table_;
  std::vector<std::string> local_variables_;
};

}  // namespace terrier::parser::udf
