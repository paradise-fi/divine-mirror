/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
// V: v.leakcheck CC_OPT: -Os V_OPT: --leakcheck exit TAGS: todo

#include <sys/lamp.h>
#include <cassert>

int x;

int get() { return x; }

int main() {
    int val = __lamp_any_i32();
    x = val;

    int y = get();
    assert( x == y );
}
