// Expected output: 45

fun main() -> int {
    var c = 0
    for (;false;c=c+1) {
        c = c + 1
        if(c > 6) {
            break
        }
    }
    return c
}
