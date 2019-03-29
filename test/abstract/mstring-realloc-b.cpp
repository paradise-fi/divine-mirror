/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    char buff1[5] = "aabb";
    char * str = __mstring_val( buff1, 5 );
    char * extended = (char *)realloc( str, 3 * sizeof( char ) );

    char buff2[4] = "aab";
    char * expected = __mstring_val( buff2, 4 );

    int  i = 0;
    while ( expected[ i ] != '\0' ) {
        assert( extended[ i ] == expected[ i ] );
        ++i;
    }
}