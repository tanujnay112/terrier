#include <array>
#include <memory>

#include "execution/sql_test.h"

#include "catalog/catalog_defs.h"
#include "execution/sql/inserter.h"
#include "execution/sql/deleter.h"
#include "execution/util/timer.h"

#include "execution/sql/index_iterator.h"

namespace terrier::execution::sql::test {

class DeleterTest : public SqlBasedTest {
  void SetUp() override {
    // Create the test tables
    SqlBasedTest::SetUp();
    exec_ctx_ = MakeExecCtx();
    GenerateTestTables(exec_ctx_.get());
  }

 protected:
  /**
   * Execution context to use for the test
   */
  std::unique_ptr<exec::ExecutionContext> exec_ctx_;
};

// NOLINTNEXTLINE
TEST_F(DeleterTest, SimpleDeleterTest) {
  
  //
  auto table_oid = exec_ctx_->GetAccessor()->GetTableOid(NSOid(), "test_1");
  Inserter inserter(exec_ctx_.get(), table_oid);
  Deleter deleter(exec_ctx_.get(), table_oid);
  auto table_pr = inserter.GetTablePR();
  auto schema = exec_ctx_->GetAccessor()->GetSchema(table_oid);

  *reinterpret_cast<int16_t *>(table_pr->AccessForceNotNull(3)) = 15;
  *reinterpret_cast<int32_t *>(table_pr->AccessForceNotNull(1)) = 721;
  *reinterpret_cast<int32_t *>(table_pr->AccessForceNotNull(2)) = 4256;
  *reinterpret_cast<int64_t *>(table_pr->AccessForceNotNull(0)) = 445;

  // Insert into table, and verify that insert succeeds
  auto tuple_slot = inserter.TableInsert();
  auto table = exec_ctx_->GetAccessor()->GetTable(table_oid);
  size_t count = 0;
  for (auto iter = table->begin(); iter != table->end(); iter++) {
    count++;
  }
  EXPECT_EQ(TEST1_SIZE + 1, count);

  auto index_oid = exec_ctx_->GetAccessor()->GetIndexOid(NSOid(), "index_1");
  auto index = exec_ctx_->GetAccessor()->GetIndex(index_oid);

  // Create Insert PR
  auto insert_pr = inserter.GetIndexPR(index_oid);
  *reinterpret_cast<int32_t *>(insert_pr->AccessForceNotNull(0)) = 1337;

  // Insert into index
  std::vector<storage::TupleSlot> results_before_insertion, results_after_insertion;
  index->ScanKey(*exec_ctx_->GetTxn(), *insert_pr, &results_before_insertion);
  EXPECT_TRUE(inserter.IndexInsert(index_oid));

  // Verify that insertion succeeds
  index->ScanKey(*exec_ctx_->GetTxn(), *insert_pr, &results_after_insertion);
  EXPECT_EQ(results_before_insertion.size() + 1, results_after_insertion.size());
  std::cerr<< "res1 " << results_before_insertion.size() << ", res2 " << results_after_insertion.size() << std::endl;

  // Create Delete PR
  auto delete_pr = deleter.GetIndexPR(index_oid);
  *reinterpret_cast<int32_t *>(delete_pr->AccessForceNotNull(0)) = 1337;

  // Delete
  std::vector<storage::TupleSlot> results_before_delete, results_after_delete;
  index->ScanKey(*exec_ctx_->GetTxn(), *delete_pr, &results_before_delete);
  deleter.TableDelete(tuple_slot);
  deleter.IndexDelete(index_oid, tuple_slot);

  // Test that table delete succeeds
  count = 0;
  for (auto iter = table->begin(); iter != table->end(); iter++) {
    count++;
  }
  EXPECT_EQ(TEST1_SIZE, count);

  // Test that index delete succeeds
  index->ScanKey(*exec_ctx_->GetTxn(), *delete_pr, &results_after_delete);
  EXPECT_EQ(results_before_delete.size() - 1, results_after_delete.size());
}

// NOLINTNEXTLINE
// TEST_F(DeleterTest, MultiIndexTest) {
//   //
//   auto table_oid = exec_ctx_->GetAccessor()->GetTableOid(NSOid(), "test_2");
//   Deleter deleter(exec_ctx_.get(), table_oid);
//   auto table_pr = deleter.GetTablePR();
//   auto schema = exec_ctx_->GetAccessor()->GetSchema(table_oid);

//   *reinterpret_cast<int16_t *>(table_pr->AccessForceNotNull(3)) = 15;
//   *reinterpret_cast<int32_t *>(table_pr->AccessForceNotNull(1)) = 721;
//   *reinterpret_cast<int32_t *>(table_pr->AccessForceNotNull(2)) = 4256;
//   *reinterpret_cast<int64_t *>(table_pr->AccessForceNotNull(0)) = 445;

//   deleter.TableInsert();

//   auto table = exec_ctx_->GetAccessor()->GetTable(table_oid);

//   size_t count = 0;
//   for (auto iter = table->begin(); iter != table->end(); iter++) {
//     count++;
//   }
//   EXPECT_EQ(TEST2_SIZE + 1, count);

//   auto index_oid = exec_ctx_->GetAccessor()->GetIndexOid(NSOid(), "index_2");
//   auto index_pr = deleter.GetIndexPR(index_oid);
//   auto index = exec_ctx_->GetAccessor()->GetIndex(index_oid);
//   *reinterpret_cast<int16_t *>(index_pr->AccessForceNotNull(0)) = 15;
//   std::vector<storage::TupleSlot> results1;
//   index->ScanKey(*exec_ctx_->GetTxn(), *index_pr, &results1);
//   EXPECT_TRUE(deleter.IndexInsert(index_oid));
//   {
//     std::vector<storage::TupleSlot> results2;
//     index->ScanKey(*exec_ctx_->GetTxn(), *index_pr, &results2);
//     EXPECT_EQ(results1.size() + 1, results2.size());
//   }

//   auto index_oid2 = exec_ctx_->GetAccessor()->GetIndexOid(NSOid(), "index_2_multi");
//   auto index_pr2 = deleter.GetIndexPR(index_oid2);
//   auto index2 = exec_ctx_->GetAccessor()->GetIndex(index_oid2);
//   *reinterpret_cast<int16_t *>(index_pr2->AccessForceNotNull(1)) = 15;
//   *reinterpret_cast<int32_t *>(index_pr2->AccessForceNotNull(0)) = 721;
//   std::vector<storage::TupleSlot> results3;
//   index2->ScanKey(*exec_ctx_->GetTxn(), *index_pr2, &results3);
//   EXPECT_TRUE(deleter.IndexInsert(index_oid2));
//   std::vector<storage::TupleSlot> results4;
//   index2->ScanKey(*exec_ctx_->GetTxn(), *index_pr2, &results4);
//   EXPECT_EQ(results3.size() + 1, results4.size());
//   {
//     std::array<uint32_t, 2> col_oids{1, 2};

//     IndexIterator index_iter{exec_ctx_.get(), !table_oid, !index_oid2, col_oids.data(),
//                              static_cast<uint32_t>(col_oids.size())};
//     index_iter.Init();

//     auto pr = index_iter.PR();

//     *reinterpret_cast<int16_t *>(pr->AccessForceNotNull(1)) = 15;
//     *reinterpret_cast<int32_t *>(pr->AccessForceNotNull(0)) = 721;
//     index_iter.ScanKey();
//     size_t final_multi_count = 0;
//     while (index_iter.Advance()) {
//       final_multi_count++;
//     }

//     EXPECT_EQ(final_multi_count, results4.size());
//   }
// }

}  // namespace terrier::execution::sql::test