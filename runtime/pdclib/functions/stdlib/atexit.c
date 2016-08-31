/* $Id$ */

/* atexit( void (*)( void ) )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdlib.h>

#ifndef REGTEST

#ifndef __divine__

extern void (*_PDCLIB_regstack[])( void );
extern size_t _PDCLIB_regptr;

int atexit( void (*func)( void ) )
{
    if ( _PDCLIB_regptr == 0 )
    {
        return -1;
    }
    else
    {
        _PDCLIB_regstack[ --_PDCLIB_regptr ] = func;
        return 0;
    }
}

#else

#include <divine.h>

extern void (**_PDCLIB_regstack)( void );
extern size_t _PDCLIB_regptr;

int atexit( void (*func)( void ) )
{
    const size_t ptrsize = sizeof( void (*)( void ) );

    if ( _PDCLIB_regptr >= 32 ) {
        void *n = realloc( _PDCLIB_regstack, ptrsize * ( _PDCLIB_regptr + 1 ) );
        if ( n )
            _PDCLIB_regstack = n;
        else
            return 1; /* ? */
    } else if ( !_PDCLIB_regstack )
        _PDCLIB_regstack = __vm_obj_make( 32 * ptrsize );

    _PDCLIB_regstack[ _PDCLIB_regptr ++ ] = func;
    return 0;
}

#endif

#endif

#ifdef TEST
#include <_PDCLIB_test.h>
#include <assert.h>

static int flags[ 32 ];

static void counthandler( void )
{
    static int count = 0;
    flags[ count ] = count;
    ++count;
}

static void checkhandler( void )
{
    for ( int i = 0; i < 31; ++i )
    {
        assert( flags[ i ] == i );
    }
}

int main( void )
{
    TESTCASE( atexit( &checkhandler ) == 0 );
    for ( int i = 0; i < 31; ++i )
    {
        TESTCASE( atexit( &counthandler ) == 0 );
    }
    return TEST_RESULTS;
}

#endif
