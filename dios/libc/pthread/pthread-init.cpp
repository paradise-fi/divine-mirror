// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.h>

using namespace __dios;

/* Initializes a thread of the main task. This should be executed before we
 * run main and call global constructors, because they may depend on thread
 * data.
 * */
void __pthread_initialize() noexcept
{
    // initialize implicitly created main thread
    __init_thread( __dios_this_task(), PTHREAD_CREATE_DETACHED );
    getThread().is_main = true;
    getThread().started = true;
}

/* Terminate and cleanup all threads except itself for the current process.
 * All threads are killed and their TLS is deallocated without calling cleanup
 * handlers.
 *
 * This function is used at exit() from process.
 * */
void __pthread_finalize() noexcept
{
    __dios::FencedInterruptMask mask;

    auto **threads = __dios_this_process_tasks();
    auto *self = __dios_this_task();
    int cnt = __vm_obj_size( threads ) / sizeof( struct __dios_tls * );
    for ( int i = 0; i < cnt; ++i )
    {
        /* FIXME this conflicts with any non-pthread tasks with non-trivial TLS */
        if ( __vm_obj_size( threads[i] ) > sizeof( struct __dios_tls ) )
            delete &getThread( threads[i] );
        if ( threads[ i ] != self )
            __dios_kill( threads[ i ] );
    }
    __vm_obj_free( threads );

    __pthread_atfork_fini();
    __pthread_tls_fini();
}


/* At start of a thread, DiOS associates the current task with the thread.
 * Hence the tid is initialized to be equal to __dios_task handle.
 *
 * The __pthread_start function executes thread entry function in an unmasked
 * environment (with enabled interrupts), and stores the result to thread storage
 * (see dios/include/sys/thread.h).
 *
 * After successful execution, thread storage is released and the thread
 * becomes zombie (see _clean_and_become_zombie).
 */
__noinline void __pthread_start( void *_args )
{
    __dios::FencedInterruptMask mask;

    Entry *args = static_cast< Entry * >( _args );
    auto tid = __dios_this_task();
    _PThread &thread = getThread( tid );

    // copy arguments
    void *arg = args->arg;
    auto entry = args->entry;
    __vm_obj_free( _args );

    thread.entry = nullptr;
    thread.started = true;

    // call entry function
    mask.without( [&] {
        thread.result = entry( arg );
    } );

    assert( thread.sleeping == false );

    _clean_and_become_zombie( mask, tid );
}

namespace __dios
{

/* Creates a thread for task `gtid`. Constructs its thread local storage
 * and sets its metadata. The resulting thread is in running state.
 *
 * Each thread, either main thread or threads created by
 * pthread_create, is initialized by this method.
 * */
void __init_thread( const __dios_task gtid, const pthread_attr_t attr ) noexcept
{
    __dios_assert( gtid );

    if ( __vm_obj_size( gtid ) < _PthreadTLS::raw_size( 0 ) )
        __vm_obj_resize( gtid, _PthreadTLS::raw_size( 0 ) );
    auto *thread = new _PThread;
    tls( gtid ).thread = thread;

    // initialize thread metadata
    thread->started = false;
    thread->running = true;
    thread->detached = ( ( attr & _THREAD_ATTR_DETACH_MASK ) == PTHREAD_CREATE_DETACHED );
    thread->condition = nullptr;
    thread->cancel_state = PTHREAD_CANCEL_ENABLE;
    thread->cancel_type = PTHREAD_CANCEL_DEFERRED;
}

/* Cleanup TLS associated with thread `tid`. For every tls key calls associated
 * cleanup handler until any such handler exists.
 *
 * If the terminating thread is main, the function exits the process. Otherwise,
 * the thread is either is in the detached state than it can be freely killed and
 * release its storage, else the thread needs to wait to be detached or joined.
 */
_Noreturn void _clean_and_become_zombie( __dios::FencedInterruptMask &mask,
                                         __dios_task tid ) noexcept
{
    _PThread &thread = getThread( tid );
    // An  optional  destructor  function may be associated with each key
    // value.  At thread exit, if a key value has a non-NULL destructor
    // pointer, and the thread has a non-NULL value associated with that key,
    // the value of the key is set to NULL, and then the function pointed to is
    // called with the previously associated value as its sole argument. The
    // order of destructor calls is unspecified if more than one destructor
    // exists for a thread when it exits.
    //
    // If, after all the destructors have been called for all non-NULL values
    // with associated destructors, there are still some non-NULL values  with
    // associated  destructors, then  the  process  is  repeated.  If,  after
    // at  least {PTHREAD_DESTRUCTOR_ITERATIONS}  iterations  of destructor
    // calls for outstanding non-NULL values, there are still some non-NULL
    // values with associated destructors, implementations may stop calling
    // destructors, or they may continue calling destructors until no
    // non-NULL values with associated destructors exist, even though this
    // might result in an infinite loop.
    int iter = 0;
    bool done;

    auto &tls = __dios::tls( tid );
    do {
        done = true;
        for ( int i = 0; i < tls.keyCount(); ++i )
            if ( tls.getKey( i ) )
            {
                done = false;
                tls.destroy( i );
            }
        ++iter;
    } while ( iter <= PTHREAD_DESTRUCTOR_ITERATIONS && !done );

    thread.running = false;

    if ( thread.is_main )
        exit( 0 );

    if ( thread.detached )
        releaseAndKillThread( tid );
    else // wait until detach / join kills us
        wait( mask, [&] { return true; } );
    __builtin_trap();
}

}
