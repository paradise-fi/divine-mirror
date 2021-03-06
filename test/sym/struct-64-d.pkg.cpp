/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>
#include <cstdint>
#include <cassert>

struct S { int64_t val; };
struct T { int64_t val; };
struct U { T t; };
struct V { S s; U u; };

int main() {
    V a, b;
    int x = __lamp_any_i32();
    a.s.val = x;
    b.u.t.val = a.s.val;
    assert( a.s.val == b.u.t.val );
}
