#include <cassert>
#include <string>

#include "lib/logging.h"

void test_get_segment() {
    assert("1" == get_segment("1.2.3.4", "."));
    assert("xxx" == get_segment("xxx.yyy.zzz", "."));
    assert("4" == get_segment("1.2.3.4", ".", 4));
    assert("zzz" == get_segment("xxx.yyy.zzz", ".", 3));
    assert(string() == get_segment("xxx.yyy.zzz", ".", 4));
}

int main() {
    test_get_segment();
    return 0;
}

