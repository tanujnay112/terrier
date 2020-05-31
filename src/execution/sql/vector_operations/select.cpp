#include "execution/sql/vector_operations/vector_operations.h"

#include "spdlog/fmt/fmt.h"

#include "execution/util/exception.h"
#include "execution/util/settings.h"
#include "execution/sql/operators/comparison_operators.h"
#include "execution/sql/runtime_types.h"
#include "execution/sql/tuple_id_list.h"

namespace terrier::execution::sql {

namespace {

// Filter optimization:
// --------------------
// When perform a comparison between two vectors we __COULD__ just iterate the input TID list, apply
// the predicate, update the list based on the result of the comparison, and be done with it. But,
// we take advantage of the fact that, for some data types, we can operate on unselected data and
// potentially leverage SIMD to accelerate performance. This only works for simple types like
// integers because unselected data can safely participate in comparisons and get masked out later.
// This is not true for complex types like strings which may have NULLs or other garbage.
//
// This "full-compute" optimization is only beneficial for a range of selectivities that depend on
// the input vector's data type. We use the is_safe_for_full_compute type trait to determine whether
// the input type supports "full-compute". If so, we also verify that the selectivity of the TID
// list warrants the optimization.

template <typename T, typename Enable = void>
struct is_safe_for_full_compute {
  static constexpr bool value = false;
};

template <typename T>
struct is_safe_for_full_compute<
    T, std::enable_if_t<std::is_fundamental_v<T> || std::is_same_v<T, Date> ||
                        std::is_same_v<T, Timestamp> || std::is_same_v<T, Decimal32> ||
                        std::is_same_v<T, Decimal64> || std::is_same_v<T, Decimal128>>> {
  static constexpr bool value = true;
};

// When performing a selection between two vectors, we need to make sure of a few things:
// 1. The types of the two vectors are the same
// 2. If both input vectors are not constants
//   2a. The size of the vectors are the same
//   2b. The selection counts (i.e., the number of "active" or "visible" elements is equal)
// 3. The output TID list is sufficiently large to represents all TIDs in both left and right inputs
void CheckSelection(const Vector &left, const Vector &right, TupleIdList *result) {
  if (left.GetTypeId() != right.GetTypeId()) {
    throw TypeMismatchException(left.GetTypeId(), right.GetTypeId(),
                                "input vector types must match for selections");
  }
  if (!left.IsConstant() && !right.IsConstant()) {
    if (left.GetSize() != right.GetSize()) {
      throw Exception(ExceptionType::Execution,
                      "left and right vectors to comparison have different sizes");
    }
    if (left.GetCount() != right.GetCount()) {
      throw Exception(ExceptionType::Execution,
                      "left and right vectors to comparison have different counts");
    }
    if (result->GetCapacity() != left.GetSize()) {
      throw Exception(ExceptionType::Execution,
                      "result list not large enough to store all TIDs in input vector");
    }
  }
}

template <typename T, typename Op>
void TemplatedSelectOperation_Vector_Constant(const Vector &left, const Vector &right,
                                              TupleIdList *tid_list) {
  // If the scalar constant is NULL, all comparisons are NULL.
  if (right.IsNull(0)) {
    tid_list->Clear();
    return;
  }

  auto *left_data = reinterpret_cast<const T *>(left.GetData());
  auto &constant = *reinterpret_cast<const T *>(right.GetData());

  // Safe full-compute. Refer to comment at start of file for explanation.
  if constexpr (is_safe_for_full_compute<T>::value) {
    const auto full_compute_threshold =
        Settings::Instance()->GetDouble(Settings::Name::SelectOptThreshold);

    if (full_compute_threshold <= tid_list->ComputeSelectivity()) {
      TupleIdList::BitVectorType *bit_vector = tid_list->GetMutableBits();
      bit_vector->UpdateFull([&](uint64_t i) { return Op{}(left_data[i], constant); });
      bit_vector->Difference(left.GetNullMask());
      return;
    }
  }

  // Remove all NULL entries from left input. Right constant is guaranteed non-NULL by this point.
  tid_list->GetMutableBits()->Difference(left.GetNullMask());

  // Filter
  tid_list->Filter([&](uint64_t i) { return Op{}(left_data[i], constant); });
}

template <typename T, typename Op>
void TemplatedSelectOperation_Vector_Vector(const Vector &left, const Vector &right,
                                            TupleIdList *tid_list) {
  auto *left_data = reinterpret_cast<const T *>(left.GetData());
  auto *right_data = reinterpret_cast<const T *>(right.GetData());

  // Safe full-compute. Refer to comment at start of file for explanation.
  if constexpr (is_safe_for_full_compute<T>::value) {
    const auto full_compute_threshold =
        Settings::Instance()->GetDouble(Settings::Name::SelectOptThreshold);

    // Only perform the full compute if the TID selectivity is larger than the threshold
    if (full_compute_threshold <= tid_list->ComputeSelectivity()) {
      TupleIdList::BitVectorType *bit_vector = tid_list->GetMutableBits();
      bit_vector->UpdateFull([&](uint64_t i) { return Op{}(left_data[i], right_data[i]); });
      bit_vector->Difference(left.GetNullMask()).Difference(right.GetNullMask());
      return;
    }
  }

  // Remove all NULL entries in either vector
  tid_list->GetMutableBits()->Difference(left.GetNullMask()).Difference(right.GetNullMask());

  // Filter
  tid_list->Filter([&](uint64_t i) { return Op{}(left_data[i], right_data[i]); });
}

template <typename T, template <typename> typename Op>
void TemplatedSelectOperation(const Vector &left, const Vector &right, TupleIdList *tid_list) {
  if (right.IsConstant()) {
    TemplatedSelectOperation_Vector_Constant<T, Op<T>>(left, right, tid_list);
  } else if (left.IsConstant()) {
    // NOLINTNEXTLINE re-arrange arguments
    TemplatedSelectOperation_Vector_Constant<T, typename Op<T>::SymmetricOp>(right, left, tid_list);
  } else {
    TemplatedSelectOperation_Vector_Vector<T, Op<T>>(left, right, tid_list);
  }
}

template <template <typename> typename Op>
void SelectOperation(const Vector &left, const Vector &right, TupleIdList *tid_list) {
  // Sanity check
  CheckSelection(left, right, tid_list);

  // Lift-off
  switch (left.GetTypeId()) {
    case TypeId::Boolean:
      TemplatedSelectOperation<bool, Op>(left, right, tid_list);
      break;
    case TypeId::TinyInt:
      TemplatedSelectOperation<int8_t, Op>(left, right, tid_list);
      break;
    case TypeId::SmallInt:
      TemplatedSelectOperation<int16_t, Op>(left, right, tid_list);
      break;
    case TypeId::Integer:
      TemplatedSelectOperation<int32_t, Op>(left, right, tid_list);
      break;
    case TypeId::BigInt:
      TemplatedSelectOperation<int64_t, Op>(left, right, tid_list);
      break;
    case TypeId::Hash:
      TemplatedSelectOperation<hash_t, Op>(left, right, tid_list);
      break;
    case TypeId::Pointer:
      TemplatedSelectOperation<uintptr_t, Op>(left, right, tid_list);
      break;
    case TypeId::Float:
      TemplatedSelectOperation<float, Op>(left, right, tid_list);
      break;
    case TypeId::Double:
      TemplatedSelectOperation<double, Op>(left, right, tid_list);
      break;
    case TypeId::Date:
      TemplatedSelectOperation<Date, Op>(left, right, tid_list);
      break;
    case TypeId::Timestamp:
      TemplatedSelectOperation<Timestamp, Op>(left, right, tid_list);
      break;
    case TypeId::Varchar:
      TemplatedSelectOperation<storage::VarlenEntry, Op>(left, right, tid_list);
      break;
    default:
      throw NotImplementedException(fmt::format("selections on vector type '{}' not supported",
                                                TypeIdToString(left.GetTypeId())));
  }
}

}  // namespace

void VectorOps::SelectEqual(const Vector &left, const Vector &right, TupleIdList *tid_list) {
  SelectOperation<terrier::execution::sql::Equal>(left, right, tid_list);
}

void VectorOps::SelectGreaterThan(const Vector &left, const Vector &right, TupleIdList *tid_list) {
  SelectOperation<terrier::execution::sql::GreaterThan>(left, right, tid_list);
}

void VectorOps::SelectGreaterThanEqual(const Vector &left, const Vector &right,
                                       TupleIdList *tid_list) {
  SelectOperation<terrier::execution::sql::GreaterThanEqual>(left, right, tid_list);
}

void VectorOps::SelectLessThan(const Vector &left, const Vector &right, TupleIdList *tid_list) {
  SelectOperation<terrier::execution::sql::LessThan>(left, right, tid_list);
}

void VectorOps::SelectLessThanEqual(const Vector &left, const Vector &right,
                                    TupleIdList *tid_list) {
  SelectOperation<terrier::execution::sql::LessThanEqual>(left, right, tid_list);
}

void VectorOps::SelectNotEqual(const Vector &left, const Vector &right, TupleIdList *tid_list) {
  SelectOperation<terrier::execution::sql::NotEqual>(left, right, tid_list);
}

}  // namespace terrier::execution::sql
