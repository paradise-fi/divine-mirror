#ifndef _SYS_STACK_H
#define _SYS_STACK_H

#include <_PDCLIB/cdefs.h>
#include <sys/cdefs.h>
#include <sys/interrupt.h>

_PDCLIB_EXTERN_C

__inline static inline struct _VM_Frame *__dios_this_frame() __nothrow
{
    return __CAST( struct _VM_Frame *, __vm_ctl_get( _VM_CR_Frame ) );
}

__inline static inline struct _VM_Frame *__dios_parent_frame() __nothrow
{
    struct _VM_Frame *f = __dios_this_frame();
    if ( f )
        return f->parent;
    else
        return 0;
}

static inline void __dios_set_frame( struct _VM_Frame *f ) __nothrow
{
    __vm_ctl_set( _VM_CR_Frame, f );
}

__inline static inline void __dios_sync_parent_frame() __nothrow
{
    void **f = __CAST( void **, __vm_ctl_get( _VM_CR_User1 ) );
    if ( !f ) return;
    struct _VM_Frame *self = __CAST( struct _VM_Frame *, __vm_ctl_get( _VM_CR_Frame ) );
    *f = self->parent;
}

__inline static inline void __dios_sync_this_frame() __nothrow
{
    void **f = __CAST( void **, __vm_ctl_get( _VM_CR_User1 ) );
    if ( f )
        *f = __dios_this_frame();
}

__noinline void __dios_stack_cut( struct _VM_Frame *top, struct _VM_Frame *bottom ) __nothrow;
__noinline void __dios_stack_free( struct _VM_Frame *from, struct _VM_Frame *to ) __nothrow;

// destroy this frame and transfer control to given frame and program counter,
// if restoreMaskTo is -1 it does not change mask
_PDCLIB_noreturn inline void __dios_jump( struct _VM_Frame *to, void (*pc)( void ),
                                          int restoreMaskTo )
    _PDCLIB_nothrow __attribute__((__always_inline__))
{
    if ( restoreMaskTo == 0 )
        __vm_ctl_flag( _DiOS_CF_Mask, 0 );
    if ( restoreMaskTo == 1 )
        __vm_ctl_flag( 0, _DiOS_CF_Mask );
    __vm_ctl_set( _VM_CR_Frame, to, pc );
    __builtin_unreachable();
}

// set value of register of instruction identified by 'pc' in frame identified
// by 'frame' 'lenght' bytes of 'value' will be written to offset 'offset' of
// the register
// triggers an assert if it was not able to write the register
void __dios_set_register( struct _VM_Frame *frame, _VM_CodePointer pc,
                         unsigned offset, uint64_t value, unsigned lenght )
                         __attribute__((__noinline__));

// similar to __dios_set_register except it reads from given (part) of register
// and returns the read value
uint64_t __dios_get_register( struct _VM_Frame *frame, _VM_CodePointer pc,
                         unsigned offset, unsigned lenght );

_PDCLIB_EXTERN_END

#endif
