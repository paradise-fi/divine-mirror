#include <errno.h>
#include <dios.h>
#include <assert.h>

int main() {
    assert( errno == 0 );
    errno = 42;
    assert( errno == 42 );
    assert( *__dios_get_errno() == 42 );
    assert( &errno == __dios_get_errno() );
}