#pragma once
#include <memory>
#include <utility>
#include "execution/exec/output.h"
#include "execution/sql/memory_pool.h"
#include "transaction/transaction_context.h"
#include "transaction/transaction_manager.h"

namespace tpl::exec {
using terrier::transaction::TransactionContext;

/**
 * Execution Context: Stores information handed in by upper layers.
 */
class ExecutionContext {
 public:
  /**
   * Constructor
   * @param txn transaction used by this query
   * @param callback callback function for outputting
   * @param final_schema the FinalSchema of the output
   */
  ExecutionContext(TransactionContext *txn, OutputCallback callback, const std::shared_ptr<FinalSchema> &final_schema)
      : txn_(txn),
        buffer_( final_schema == nullptr ? nullptr :
            std::make_unique<OutputBuffer>(final_schema->GetCols().size(), ComputeTupleSize(final_schema), callback)) {}

  /**
   * @return the transaction used by this query
   */
  TransactionContext *GetTxn() { return txn_; }

  /**
   * @return the output buffer used by this query
   */
  OutputBuffer *GetOutputBuffer() { return buffer_.get(); }

  /**
   * @return the memory pool
   */
  sql::MemoryPool *GetMemoryPool() { return mem_pool_.get(); }

  /**
   * Set the memory pool
   * @param mem_pool new memory pool
   */
  void SetMemoryPool(std::unique_ptr<sql::MemoryPool> &&mem_pool) { mem_pool_ = std::move(mem_pool); }

  /**
   * @param final_schema the FinalSchema of the output
   * @return the size of tuple with this final_schema
   */
  static uint32_t ComputeTupleSize(const std::shared_ptr<FinalSchema> &final_schema) {
    uint32_t tuple_size = 0;
    for (const auto &col : final_schema->GetCols()) {
      tuple_size += sql::ValUtil::GetSqlSize(col.GetType());
    }
    return tuple_size;
  }

 private:
  TransactionContext *txn_;
  std::unique_ptr<OutputBuffer> buffer_;
  std::unique_ptr<sql::MemoryPool> mem_pool_{nullptr};
};
}  // namespace tpl::exec
