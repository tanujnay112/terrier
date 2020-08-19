#include "execution/compiler/operator/iter_cte_scan_leader_translator.h"

#include <execution/compiler/loop.h>

#include "execution/compiler/codegen.h"
#include "execution/compiler/compilation_context.h"
#include "execution/compiler/function_builder.h"
#include "execution/compiler/work_context.h"
#include "parser/expression/constant_value_expression.h"
//
namespace terrier::execution::compiler {

IterCteScanLeaderTranslator::IterCteScanLeaderTranslator(const planner::CteScanPlanNode &plan,
                                                         CompilationContext *compilation_context, Pipeline *pipeline)
    : OperatorTranslator(plan, compilation_context, pipeline, brain::ExecutionOperatingUnitType::CTE_SCAN),
      op_(&plan),
      col_types_(GetCodeGen()->MakeFreshIdentifier("col_types")),
      col_oids_var_(GetCodeGen()->MakeFreshIdentifier("col_oids")),
      insert_pr_(GetCodeGen()->MakeFreshIdentifier("insert_pr")),
      base_pipeline_(this, Pipeline::Parallelism::Parallel),
      build_pipeline_(this, Pipeline::Parallelism::Parallel) {
  auto iter_cte_type = GetCodeGen()->BuiltinType(ast::BuiltinType::Kind::IterCteScanIterator);
  auto cte_type = GetCodeGen()->BuiltinType(ast::BuiltinType::Kind::CteScanIterator);
  cte_scan_val_entry_ = compilation_context->GetQueryState()->DeclareStateEntry(
      GetCodeGen(), op_->GetCTETableName() + "val", iter_cte_type);
  cte_scan_ptr_entry_ = compilation_context->GetQueryState()->DeclareStateEntry(
      GetCodeGen(), op_->GetCTETableName() + "ptr", GetCodeGen()->PointerType(cte_type));

  pipeline->LinkNestedPipeline(&build_pipeline_);
  pipeline->LinkNestedPipeline(&base_pipeline_);
  compilation_context->Prepare(*(plan.GetChild(1)), &base_pipeline_);
  compilation_context->Prepare(*(plan.GetChild(0)), &build_pipeline_);
}

void IterCteScanLeaderTranslator::TearDownQueryState(FunctionBuilder *function) const {
  ast::Expr *cte_free_call =
      GetCodeGen()->CallBuiltin(ast::Builtin::CteScanFree, {cte_scan_ptr_entry_.Get(GetCodeGen())});
  function->Append(GetCodeGen()->MakeStmt(cte_free_call));
}

void IterCteScanLeaderTranslator::PerformPipelineWork(WorkContext *context, FunctionBuilder *function) const {
  if (&context->GetPipeline() == &base_pipeline_) {
    DeclareInsertPR(function);
    GetInsertPR(function);
    FillPRFromChild(context, function, 1);
    GenTableInsert(function);
    PopulateReadCteScanIterator(function);
    return;
  }

  if (&context->GetPipeline() != &build_pipeline_) {
    auto calls = base_pipeline_.CallRunPipelineFunction();
    for (auto call : calls) {
      function->Append(call);
    }
    GenInductiveLoop(context, function);
    context->Push(function);
    return;
  }

  // Declare & Get table PR
  DeclareInsertPR(function);
  GetInsertPR(function);

  // Set the values to insert
  FillPRFromChild(context, function, 0);

  // Insert into table
  GenTableInsert(function);
  context->Push(function);
}

void IterCteScanLeaderTranslator::DeclareIterCteScanIterator(FunctionBuilder *builder) const {
  // Generate col types
  auto codegen = GetCodeGen();
  SetColumnTypes(builder);
  SetColumnOids(builder);
  auto &plan = GetPlanAs<planner::CteScanPlanNode>();
  // Call @cteScanIteratorInit
  ast::Expr *cte_scan_iterator_setup = codegen->IterCteScanIteratorInit(
      cte_scan_val_entry_.GetPtr(codegen), plan.GetTableOid(), col_oids_var_, col_types_, plan.GetIsRecursive(),
      GetCompilationContext()->GetExecutionContextPtrFromQueryState());
  builder->Append(codegen->MakeStmt(cte_scan_iterator_setup));

  ast::Stmt *pointer_setup =
      codegen->Assign(cte_scan_ptr_entry_.Get(codegen),
                      codegen->CallBuiltin(ast::Builtin::IterCteScanGetReadCte, {cte_scan_val_entry_.GetPtr(codegen)}));
  builder->Append(pointer_setup);
}

void IterCteScanLeaderTranslator::SetColumnTypes(FunctionBuilder *builder) const {
  // Declare: var col_types: [num_cols]uint32
  auto codegen = GetCodeGen();
  auto size = op_->GetTableSchema()->GetColumns().size();
  ast::Expr *arr_type = codegen->ArrayType(size, ast::BuiltinType::Kind::Uint32);
  builder->Append(codegen->DeclareVar(col_types_, arr_type, nullptr));

  // For each oid, set col_oids[i] = col_oid
  for (uint16_t i = 0; i < size; i++) {
    ast::Expr *lhs = codegen->ArrayAccess(col_types_, i);
    ast::Expr *rhs = codegen->Const32(static_cast<uint32_t>(op_->GetTableSchema()->GetColumns()[i].Type()));
    builder->Append(codegen->Assign(lhs, rhs));
  }
}

void IterCteScanLeaderTranslator::SetColumnOids(FunctionBuilder *builder) const {
  // Declare: var col_types: [num_cols]uint32
  auto codegen = GetCodeGen();
  auto size = op_->GetTableSchema()->GetColumns().size();
  ast::Expr *arr_type = codegen->ArrayType(size, ast::BuiltinType::Kind::Uint32);
  builder->Append(codegen->DeclareVar(col_oids_var_, arr_type, nullptr));

  // For each oid, set col_oids[i] = col_oid
  for (uint16_t i = 0; i < size; i++) {
    ast::Expr *lhs = codegen->ArrayAccess(col_oids_var_, i);
    ast::Expr *rhs = codegen->Const32(static_cast<uint32_t>(op_->GetTableSchema()->GetColumns()[i].Oid()));
    builder->Append(codegen->Assign(lhs, rhs));
  }
}

void IterCteScanLeaderTranslator::InitializeQueryState(FunctionBuilder *function) const {
  DeclareIterCteScanIterator(function);
}

ast::Expr *IterCteScanLeaderTranslator::GetCteScanPtr(CodeGen *codegen) const {
  return cte_scan_ptr_entry_.Get(codegen);
}

void IterCteScanLeaderTranslator::DeclareInsertPR(terrier::execution::compiler::FunctionBuilder *builder) const {
  // var insert_pr : *ProjectedRow
  auto codegen = GetCodeGen();
  auto pr_type = codegen->BuiltinType(ast::BuiltinType::Kind::ProjectedRow);
  builder->Append(codegen->DeclareVar(insert_pr_, codegen->PointerType(pr_type), nullptr));
}

void IterCteScanLeaderTranslator::GetInsertPR(terrier::execution::compiler::FunctionBuilder *builder) const {
  // var insert_pr = cteScanGetInsertTempTablePR(...)
  auto codegen = GetCodeGen();
  auto get_pr_call =
      codegen->CallBuiltin(ast::Builtin::IterCteScanGetInsertTempTablePR, {cte_scan_val_entry_.GetPtr(codegen)});
  builder->Append(codegen->Assign(codegen->MakeExpr(insert_pr_), get_pr_call));
}

void IterCteScanLeaderTranslator::GenTableInsert(FunctionBuilder *builder) const {
  // var insert_slot = @cteScanTableInsert(&inserter_)
  auto codegen = GetCodeGen();
  auto insert_slot = codegen->MakeFreshIdentifier("insert_slot");
  auto insert_call = codegen->CallBuiltin(ast::Builtin::IterCteScanTableInsert, {cte_scan_val_entry_.GetPtr(codegen)});
  builder->Append(codegen->DeclareVar(insert_slot, nullptr, insert_call));
}

void IterCteScanLeaderTranslator::FillPRFromChild(WorkContext *context, FunctionBuilder *builder,
                                                  uint32_t child_idx) const {
  const auto &cols = op_->GetTableSchema()->GetColumns();
  auto codegen = GetCodeGen();

  for (uint32_t i = 0; i < cols.size(); i++) {
    const auto &table_col = cols[i];
    const auto &table_col_oid = table_col.Oid();
    auto val = GetChildOutput(context, child_idx, i);
    // TODO(Rohan): Figure how to get the general schema of a child node in case the field is Nullable
    // Right now it is only Non Null
    auto pr_set_call = codegen->PRSet(codegen->MakeExpr(insert_pr_), table_col.Type(), table_col.Nullable(),
                                      !EXTRACT_OID(catalog::col_oid_t, table_col_oid), val, true);
    builder->Append(codegen->MakeStmt(pr_set_call));
  }
}

void IterCteScanLeaderTranslator::FinalizeReadCteScanIterator(FunctionBuilder *builder) const {
  ast::Expr *cte_scan_iterator_setup =
      GetCodeGen()->CallBuiltin(ast::Builtin::IterCteScanGetResult, {cte_scan_val_entry_.GetPtr(GetCodeGen())});
  ast::Stmt *assign = GetCodeGen()->Assign(cte_scan_ptr_entry_.Get(GetCodeGen()), cte_scan_iterator_setup);
  builder->Append(assign);
}

void IterCteScanLeaderTranslator::GenInductiveLoop(WorkContext *context, FunctionBuilder *builder) const {
  Loop loop(builder, nullptr,
            GetCodeGen()->CallBuiltin(ast::Builtin::IterCteScanAccumulate, {cte_scan_val_entry_.GetPtr(GetCodeGen())}),
            nullptr);
  {
    PopulateReadCteScanIterator(builder);
    auto calls = build_pipeline_.CallRunPipelineFunction();
    for (auto call : calls) {
      builder->Append(call);
    }
  }
  loop.EndLoop();

  FinalizeReadCteScanIterator(builder);
}

void IterCteScanLeaderTranslator::PopulateReadCteScanIterator(FunctionBuilder *builder) const {
  ast::Expr *cte_scan_iterator_setup =
      GetCodeGen()->CallBuiltin(ast::Builtin::IterCteScanGetReadCte, {cte_scan_val_entry_.GetPtr(GetCodeGen())});
  ast::Stmt *assign = GetCodeGen()->Assign(cte_scan_ptr_entry_.Get(GetCodeGen()), cte_scan_iterator_setup);
  builder->Append(assign);
}

}  // namespace terrier::execution::compiler