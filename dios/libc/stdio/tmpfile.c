/* tmpfile( void )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>

#ifndef REGTEST

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "_PDCLIB/glue.h"
#include <unistd.h>
#ifdef __linux__
/* get O_CLOEXEC without sys/types.h being awful */
#include <asm/fcntl.h>
int open(const char *fname, int flags, ...);
#else
#include <fcntl.h>
#endif
#include <errno.h>
#include <sys/syswrap.h>

extern const _PDCLIB_fileops_t _PDCLIB_fileops;

FILE* _PDCLIB_nothrow tmpfile( void )
{

    /* Good quality random source */
    int urandom = open( "/dev/urandom", O_RDONLY | O_CLOEXEC );

    int fd;
    char filename[ L_tmpnam ];
    for ( ;; )
    {
        long long randnum;
        if( urandom == -1 || __libc_read(urandom, &randnum, sizeof randnum ) != sizeof randnum )
            randnum = rand();

        sprintf( filename, "/tmp/%llx.tmp", randnum );
        /* Check if file of this name exists. Note that fopen() is a very weak
           check, which does not take e.g. access permissions into account
           (file might exist but not readable). Replace with something more
           appropriate.
        */
        fd = open( filename, O_CREAT | O_EXCL | O_RDWR, 0600 );
        /* if it fails for other reason then existence of the file (such as
           nonexistence of /tmp/) fail immediatelly.
        */
        if ( fd == -1 && errno != EEXIST )
        {
            if ( urandom != -1 )
                __libc_close( urandom );
            return NULL;
        }
        if ( fd != -1 )
        {
            break;
        }
    }
    if ( urandom != -1 )
        __libc_close( urandom );

    FILE* rc = _PDCLIB_fvopen(((_PDCLIB_fd_t){ .sval = fd}), &_PDCLIB_fileops,
                                _PDCLIB_FWRITE | _PDCLIB_FRW |
                                _PDCLIB_DELONCLOSE, filename);
    if( rc == NULL )
    {
        __libc_close( fd );
        return NULL;
    }

    return rc;
}

#endif

#ifdef TEST
#include "_PDCLIB_test.h"
#include <string.h>

int main( void )
{
    FILE * fh;
#ifndef REGTEST
    char filename[ L_tmpnam ];
    FILE * fhtest;
#endif
    TESTCASE( ( fh = tmpfile() ) != NULL );
    TESTCASE( fputc( 'x', fh ) == 'x' );
    /* Checking that file is actually there */
    TESTCASE_NOREG( strcpy( filename, fh->filename ) == filename );
    TESTCASE_NOREG( ( fhtest = fopen( filename, "r" ) ) != NULL );
    TESTCASE_NOREG( fclose( fhtest ) == 0 );
    /* Closing tmpfile */
    TESTCASE( fclose( fh ) == 0 );
    /* Checking that file was deleted */
    TESTCASE_NOREG( fopen( filename, "r" ) == NULL );
    return TEST_RESULTS;
}

#endif

