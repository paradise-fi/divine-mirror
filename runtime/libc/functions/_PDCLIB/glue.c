// -*- C -*- (c) 2013 Petr Rockai <me@mornfall.net>
#include <limits.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <_PDCLIB_aux.h>
#include <sys/divm.h>
#include <sys/syscall.h>
#include <sys/start.h>
#include <sys/vmutil.h>
#include <string.h>
#include <dios.h>

/*
 * Glue code that ties various bits of C and C++ runtime to the divine runtime
 * support. It's not particularly pretty. Other bits of this code are also
 * exploded over runtime/ which is even worse.
 */

#define __vm_mask(x) ( ( (uintptr_t)                                    \
                           __vm_control( _VM_CA_Get, _VM_CR_Flags,      \
                                         _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, \
                                         (x) ? _VM_CF_Mask : 0 ) ) & _VM_CF_Mask )

/* Memory allocation */
void * malloc( size_t size )
{
    int masked = __vm_mask( 1 );
    void *r;
    int ok = __dios_sim_fail( _DiOS_SF_Malloc ) ? __vm_choose( 2 ) : 1;
    if ( ok )
        r = __vm_obj_make( size ); // success
    else
        r = NULL; // failure
    __vm_mask( masked );
    return r;
}

#define MIN( a, b )   ((a) < (b) ? (a) : (b))

void *realloc( void *orig, size_t size )
{
    int masked = __vm_mask( 1 );
    int ok = __dios_sim_fail( _DiOS_SF_Malloc ) ? __vm_choose( 2 ) : 1;
    void *r;
    if ( !size ) {
        __vm_obj_free( orig );
        r = NULL;
    }
    else if ( ok ) {
        void *n = __vm_obj_make( size );
        if ( orig ) {
            memcpy( n, orig, MIN( size , (size_t) __vm_obj_size( orig ) ) );
            __vm_obj_free( orig );
        }
        r = n;
    } else
        r = NULL; // failure
    __vm_mask( masked );
    return r;
}

void *calloc( size_t n, size_t size )
{
    int masked = __vm_mask( 1 );
    void *r;
    int ok = __dios_sim_fail( _DiOS_SF_Malloc ) ? __vm_choose( 2 ) : 1;
    if ( ok ) {
        void *mem = __vm_obj_make( n * size ); // success
        memset( mem, 0, n * size );
        r = mem;
    } else
        r = NULL; // failure
    __vm_mask( masked );
    return r;
}

__attribute__((__nothrow__))
void free( void * p) { if ( p ) __vm_obj_free( p ); }

/* IOStream */

void *__dso_handle; /* this is emitted by clang for calls to __cxa_exit for whatever reason */

void _exit( int rv )
{
    if ( rv )
    {
        __dios_trace_f( "Non-zero exit code: %d", rv );
        __dios_fault( _DiOS_F_ExitFault, "exit called with non-zero value" );
    }
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask | _VM_CF_Interrupted, _VM_CF_Interrupted );
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask );
    __cxa_finalize( 0 );
    __dios_run_dtors();
    __dios_syscall( SYS_exit_process, NULL, rv );
    __builtin_unreachable();
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
    (void) req;
    (void) rem;
    // I believe we will do nothing wrong if we verify nanosleep as NOOP,
    // it does not guearantee anything anyway
    return 0;
}

unsigned int sleep( unsigned int seconds )
{
    (void) seconds;
    // same as nanosleep
    return 0;
}

int usleep( useconds_t usec )
{
    (void) usec;
    return 0;
}

/* signals */

int raise( int sig )
{
    switch ( sig )
    {
        case SIGKILL:
            __vmutil_interrupt();
        default:
            return kill( getpid(), sig );
    }
}

void sigemptyset( sigset_t * sig )
{
    *sig = 0;
}

typedef void ( *SignalHandler )( int );

SignalHandler signal( int sig, SignalHandler handler )
{
    struct sigaction sa, res;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; /* Restart functions if
                                interrupted by handler */
    if ( sigaction( sig, &sa, &res ) == -1 )
        return SIG_ERR;
    else
        return res.sa_handler;
}