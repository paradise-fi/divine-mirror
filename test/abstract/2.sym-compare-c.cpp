/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <cassert>

int foo() { return 0; }

int main() {
    _SYM int x;
    // returned value should be lifted
    assert( x != foo() ); /* ERROR */
}