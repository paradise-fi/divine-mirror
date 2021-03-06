/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <string.h>

int main() {
    char input[ 10 ] =    { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0 };
    char output[ 10 ];
    char expected[ 10 ] = { 0x20, 0x10, 0x40, 0x30, 0x60, 0x50, 0x80, 0x70, 0xA0, 0x90 };

    swab( input, output, 10 );
    assert( memcmp( output, expected, 10 ) == 0 );

    return 0;
}
