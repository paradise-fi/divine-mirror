/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic */

#include <rst/domains.h>
#include <stdio.h>
#include <assert.h>

#include "common.h"

int main()
{
    char c = lower();
    assert( fputc( c, stdout ) == c );
}
