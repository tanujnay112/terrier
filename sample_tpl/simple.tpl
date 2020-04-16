// Should return 1

fun sample() -> Integer {
    var ret = IntToSql(325)
    return ret
}

fun main() -> int32 {
  var s = sample()
  return s.val
}
