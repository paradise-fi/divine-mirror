/* Includes */
#include <divine.h>
#include <cstdlib>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

/* Memory allocation */
void * malloc( size_t size ) throw() {
    __divine_interrupt_mask();
#ifdef TRACE
    trace( "malloc( %d ) called..", size );
#endif
    __divine_interrupt_mask();
#ifdef NO_MALLOC_FAILURE
    return __divine_malloc( size ); // always success
#else
    if ( __divine_choice( 2 ) ) {
        return __divine_malloc( size ); // success
    } else {
        return NULL; // failure
    }
#endif
}

/* TODO malloc currently gives zeroed memory */
void *calloc( size_t n, size_t size ) throw() { return malloc( n * size ); }
void *realloc( void *ptr, size_t size ) throw() { __divine_assert( 0 ); return 0; }

void free( void * p) throw() {
    __divine_interrupt_mask();
#ifdef TRACE
    trace( "free( %p ) called..", p );
#endif
    __divine_free( p );
}

/* IOStream */

void _ZNSt8ios_base4InitC1Ev( void ) { // std::ios_base::Init
    // TODO?
}

void *__dso_handle; /* this is emitted by clang for calls to __cxa_exit for whatever reason */

extern "C" int __cxa_atexit( void ( *func ) ( void * ), void *arg, void *dso_handle ) {
    // TODO
    ( void ) func;
    ( void ) arg;
    ( void ) dso_handle;
    return 0;
}

extern "C" void abort_message( const char * ) { __divine_assert( 0 ); }
extern "C" void *dlsym( void *, void * ) { __divine_assert( 0 ); return 0; } // oh golly...

extern "C" void *__errno_location() { __divine_assert( 0 ); return 0; }

extern "C" { /* POSIX kernel APIs */

    void raise( int ) { __divine_assert( 0 ); }
    int unlink( const char * ) { __divine_assert( 0 ); return 0; }
    ssize_t read(int, void *, size_t) { __divine_assert( 0 ); return 0; }
    ssize_t write(int, const void *, size_t) { __divine_assert( 0 ); return 0; }
    off_t lseek(int, off_t, int) { __divine_assert( 0 ); return 0; }
    int close(int) { __divine_assert( 0 ); return 0; }

}

/* currently missing from our pdclib */
extern "C" FILE *tmpfile() throw() { __divine_assert( 0 ); return 0; }


extern "C" { /* pdclib glue functions */
    int _PDCLIB_rename( const char *, const char * ) { __divine_assert( 0 ); return 0; }
    int _PDCLIB_open( const char *, int ) { __divine_assert( 0 ); return 0; }
    void _PDCLIB_Exit( int rv ) {
        __divine_assert( !rv );
        __divine_unwind( INT_MIN );
    }
}

