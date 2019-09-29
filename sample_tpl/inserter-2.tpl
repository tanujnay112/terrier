
fun index_count_2(execCtx: *ExecutionContext, key : int64, key2 : int64) -> int64 {
  var count = 0 // output count
  // The following code initializes the index iterator.
  // The oids are the table col_oids that will be selected
  var index : IndexIterator
  var col_oids: [2]uint32
  col_oids[0] = 1 // colA
  col_oids[1] = 2 // colB
  @indexIteratorInitBind(&index, execCtx, "test_2", "index_2_multi", col_oids)

  // Next we fill up the index's projected row
  var index_pr : *ProjectedRow = @indexIteratorGetPR(&index)

  @prSetInt(index_pr, 0, @intToSql(0)) // This seems to be required for the index to find matches
                                        // (see inserter_test.cpp)

  @prSetSmallInt(index_pr, 0, @intToSql(key)) // Set colA
  @prSetInt(index_pr, 1, @intToSql(key2)) // Set colB

  // Now we iterate through the matches
  for (@indexIteratorScanKey(&index); @indexIteratorAdvance(&index);) {
    count = count + 1
  }
  // Finalize output
  @indexIteratorFree(&index)
  return count
}

fun index_count_1(execCtx: *ExecutionContext, key : int64) -> int64 {
  var count = 0 // output count
  // The following code initializes the index iterator.
  // The oids are the table col_oids that will be selected
  var index : IndexIterator
  var col_oids: [1]uint32
  col_oids[0] = 1 // colA
  @indexIteratorInitBind(&index, execCtx, "test_2", "index_2", col_oids)

  // Next we fill up the index's projected row
  var index_pr = @indexIteratorGetPR(&index)
  @prSetSmallInt(&index_pr, 0, @intToSql(key)) // Set colA

  // Now we iterate through the matches
  for (@indexIteratorScanKey(&index); @indexIteratorAdvance(&index);) {
    count = count + 1
  }
  // Finalize output
  @indexIteratorFree(&index)
  return count
}


fun main(execCtx: *ExecutionContext) -> int64 {
  var count = 0 // output count
  // The following code initializes the index iterator.
  // The oids are the table col_oids that will be selected

  var tvi: TableVectorIterator
  var oids: [4]uint32
  oids[0] = 1 // colA
  oids[1] = 2 // colB
  oids[2] = 3 // colC
  oids[3] = 4 // colD

  @tableIterInitBind(&tvi, execCtx, "test_2", oids)
  var count1 : int64
  var f : int64
  count1 = 0


  var inserter : Inserter
  @inserterInitBind(&inserter, execCtx, "test_2")
  var table_pr : *ProjectedRow = @inserterGetTablePR(&inserter)
  @prSetSmallInt(table_pr, 0, @intToSql(15))
  @prSetInt(table_pr, 1, @intToSql(14))
  @prSetBigInt(table_pr, 2, @intToSql(0))
  @prSetInt(table_pr, 3, @intToSql(48))

  for (@tableIterAdvance(&tvi)) {
    var pci = @tableIterGetPCI(&tvi)
        for (; @pciHasNext(pci); @pciAdvance(pci)) {
          count1 = count1 + 1
        }
        @pciReset(pci)
  }
  @tableIterClose(&tvi)

  var ts : TupleSlot = @inserterTableInsert(&inserter)
  var index_pr : *ProjectedRow = @inserterGetIndexPRBind(&inserter, "index_2")
  @prSetSmallInt(index_pr, 0, @prGetSmallInt(table_pr, 0))

  var index_count_before = index_count_1(execCtx, 15)
  var index_count_before_2 = index_count_2(execCtx, 15, 14)

  @inserterIndexInsertBind(&inserter, "index_2")

  @tableIterInitBind(&tvi, execCtx, "test_2", oids)
  var count2 : int64
  count2 = 0

  var c = 0
  for (@tableIterAdvance(&tvi)) {
      var pci = @tableIterGetPCI(&tvi)
          for (; @pciHasNext(pci); @pciAdvance(pci)) {
            count2 = count2 + 1
          }
          @pciReset(pci)
    }
  @tableIterClose(&tvi)

 var index_pr_2 : *ProjectedRow = @inserterGetIndexPRBind(&inserter, "index_2_multi")
 @prSetSmallInt(index_pr_2, 0, @prGetSmallInt(table_pr, 0))
 @prSetInt(index_pr_2, 1, @prGetInt(table_pr, 1))
 @inserterIndexInsertBind(&inserter, "index_2_multi")


 var index_count_after = index_count_1(execCtx, 15)
 var index_count_after_2 = index_count_2(execCtx, 15, 14)
 return (count2 - count1) + (index_count_after - index_count_before) + (index_count_after_2 - index_count_before_2)
}