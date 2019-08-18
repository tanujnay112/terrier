#include <chrono>  // NOLINT
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <thread>  // NOLINT
#include <utility>
#include <vector>

#include "execution/sql_test.h"

#include "catalog/catalog.h"
#include "execution/sql/filter_manager.h"
#include "execution/sql/table_vector_iterator.h"
#include "type/type_id.h"

namespace terrier::execution::sql::test {

class FilterManagerTest : public SqlBasedTest {
  void SetUp() override {
    // Create the test tables
    SqlBasedTest::SetUp();
    exec_ctx_ = MakeExecCtx();
    sql::TableGenerator table_generator{exec_ctx_.get(), BlockStore(), NSOid()};
    table_generator.GenerateTestTables();
  }

 protected:
  /**
   * Execution context to use for the test
   */
  std::unique_ptr<exec::ExecutionContext> exec_ctx_;
};

enum Col : uint8_t { A = 0, B = 1, C = 2, D = 3 };

uint32_t TaaT_Lt_500(ProjectedColumnsIterator *pci) {
  pci->RunFilter([pci]() -> bool {
    auto cola = *pci->Get<int32_t, false>(Col::A, nullptr);
    return cola < 500;
  });
  return pci->num_selected();
}

uint32_t Hobbled_TaaT_Lt_500(ProjectedColumnsIterator *pci) {
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  return TaaT_Lt_500(pci);
}

uint32_t Vectorized_Lt_500(ProjectedColumnsIterator *pci) {
  ProjectedColumnsIterator::FilterVal param{.i = 500};
  return pci->FilterColByVal<std::less>(Col::A, type::TypeId ::INTEGER, param);
}

// NOLINTNEXTLINE
TEST_F(FilterManagerTest, SimpleFilterManagerTest) {
  FilterManager filter(bandit::Policy::Kind::FixedAction);
  filter.StartNewClause();
  filter.InsertClauseFlavor(TaaT_Lt_500);
  filter.InsertClauseFlavor(Vectorized_Lt_500);
  filter.Finalize();
  auto table_oid = exec_ctx_->GetAccessor()->GetTableOid(NSOid(), "test_1");
  std::array<uint32_t, 1> col_oids{1};
  TableVectorIterator tvi(exec_ctx_.get(), !table_oid, col_oids.data(), static_cast<uint32_t>(col_oids.size()));
  for (tvi.Init(); tvi.Advance();) {
    auto *pci = tvi.projected_columns_iterator();

    // Run the filters
    filter.RunFilters(pci);

    // Check
    pci->ForEach([pci]() {
      auto cola = *pci->Get<int32_t, false>(Col::A, nullptr);
      EXPECT_LT(cola, 500);
    });
  }
}

// NOLINTNEXTLINE
TEST_F(FilterManagerTest, AdaptiveFilterManagerTest) {
  FilterManager filter(bandit::Policy::Kind::EpsilonGreedy);
  filter.StartNewClause();
  filter.InsertClauseFlavor(Hobbled_TaaT_Lt_500);
  filter.InsertClauseFlavor(Vectorized_Lt_500);
  filter.Finalize();
  auto table_oid = exec_ctx_->GetAccessor()->GetTableOid(NSOid(), "test_1");
  std::array<uint32_t, 1> col_oids{1};
  TableVectorIterator tvi(exec_ctx_.get(), !table_oid, col_oids.data(), static_cast<uint32_t>(col_oids.size()));
  for (tvi.Init(); tvi.Advance();) {
    auto *pci = tvi.projected_columns_iterator();

    // Run the filters
    filter.RunFilters(pci);

    // Check
    pci->ForEach([pci]() {
      auto cola = *pci->Get<int32_t, false>(Col::A, nullptr);
      EXPECT_LT(cola, 500);
    });
  }

  // The vectorized filter better be the optimal!
  EXPECT_EQ(1u, filter.GetOptimalFlavorForClause(0));
}

}  // namespace terrier::execution::sql::test
