/* VERIFY_OPTS: --symbolic */

#include <cassert>
#include <limits>
#include <sys/vmutil.h>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

int zero( int a ) {
    __vmutil_interrupt(); /* FIXME! */
    if ( a % 2 == 0 )
        return 42;
    else
        return zero( a - 1 );
}

int main() {
    __sym int a;
    assert( zero( a ) == 42 );
    return 0;
}
