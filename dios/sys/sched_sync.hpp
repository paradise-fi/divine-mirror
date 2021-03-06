// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_SYNC_SCHEDULING_H__
#define __DIOS_SYNC_SCHEDULING_H__

#include <dios/sys/sched_base.hpp>

namespace __dios {

template < typename Next >
struct SyncScheduler : public Scheduler< Next >
{
    using Task = __dios::task< typename Scheduler< Next >::Process >;

    template < typename Setup >
    void setup( Setup s ) {
        traceAlias< SyncScheduler >( "{Scheduler}" );
        s.proc1->globals = __vm_ctl_get( _VM_CR_Globals );
        s.proc1->pid = 1;

        _setupTask.reset( new Task( s.pool->get(), s.pool->get(),
                                    __dios_start_synchronous, 0, s.proc1 ) );
        assert( _setupTask );
        auto argv = construct_main_arg( "arg.", s.env, true );
        auto envp = construct_main_arg( "env.", s.env );
        this->setupMainTask( _setupTask.get(), argv.first, argv.second, envp.second );

        __vm_ctl_set( _VM_CR_Scheduler,
                      reinterpret_cast< void * >( runScheduler< typename Setup::Context > ) );

        this->setupDebug( s, argv, envp );
        Next::setup( s );
    }

    Task* getCurrentTask()
    {
        if ( _setupTask )
            return _setupTask.get();
        auto tid = __dios_this_task();
        return this->tasks.find( tid );
    }

    void yield()
    {
        void *sched = __vm_ctl_get( _VM_CR_User4 );
        getCurrentTask()->_frame = this->sysenter( false );
        __vm_ctl_set( _VM_CR_Frame, sched ); /* jump back into the scheduler */
    }

    __dios_task start_task( __dios_task_routine routine, void * arg, int tls_size )
    {
        if ( !_setupTask )
            __dios_fault( _VM_F_Control, "Cannot start task outside setup" );
        auto t_obj = new Task( routine, tls_size, _setupTask->_proc );
        auto t = this->tasks.emplace_back( t_obj ).get();
        this->setupTask( t, arg );
        return t->get_id();
    }

    __inline void run( Task& t ) noexcept
    {
        __vm_ctl_set( _VM_CR_User2, t.get_id() );

         // In synchronous mode, interrupts don't happen, but RESCHEDULE simply
         // calls __dios_interrupt and we need to prevent that.
        __vm_ctl_flag( _VM_CF_KernelMode, _VM_CF_KeepFrame | _DiOS_CF_Mask );
        __vm_ctl_set( _VM_CR_Frame, t._frame );
    }

    template < typename Context >
    static void runScheduler() noexcept
    {
        auto &scheduler = get_state< Context >();
        __vm_ctl_set( _VM_CR_User4, __dios_this_frame() );
        __vm_ctl_set( _VM_CR_User3, scheduler.debug );

        if ( scheduler._setupTask )
        {
            scheduler.run( *scheduler._setupTask );
            /* make sure task destructor can see we are not reusing
             * _setupTask's stack */
            __vm_ctl_set( _VM_CR_User2, nullptr );
            scheduler._setupTask.reset( nullptr );
            __vm_suspend();
        }

        for ( int i = 0; i < scheduler.tasks.size(); i++ )
        {
            auto& t = scheduler.tasks[ i ];
            scheduler.run( *t );
        }

        scheduler.runMonitors();
        if ( scheduler.tasks.empty() )
            scheduler.finalize();
        __vm_suspend();
    }

    std::unique_ptr< Task > _setupTask;
};

} // namespace __dios

#endif
