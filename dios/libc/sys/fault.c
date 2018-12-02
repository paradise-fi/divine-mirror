#include <sys/fault.h>
#include <sys/trace.h>
#include <sys/stack.h>
#include <sys/divm.h>

uint8_t __dios_simfail_flags;

void __dios_fault( int f, const char *msg )
{
    if( msg )
        __dios_trace_f( "FAULT: %s", msg );
    __vm_fault_t fh = ( __vm_fault_t ) __vm_ctl_get( _VM_CR_FaultHandler );
    struct _VM_Frame *frame = __dios_this_frame();
    ( *fh )( f, frame->parent, frame->parent->pc );
}
