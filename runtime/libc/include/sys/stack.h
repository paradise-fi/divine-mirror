#ifndef _SYS_STACK_H
#define _SYS_STACK_H

#include <_PDCLIB_aux.h>

_PDCLIB_EXTERN_C

// unwind and free frames on stack 'stack' from 'from' to 'to' so that 'to'
// the frame which originally returned to 'from' now returns to 'to'
// * 'stack' can be nullptr if unwinding on local stack
// * 'from' can be nullptr if everything from the caller of __dios_unwind should be unwound
// * 'to' can be nullptr if all frames from 'from' below should be destroyed
//
// i.e. __dios_unwind( nullptr, nullptr, nullptr ) destroys complete stack
// except for the caller of __dios_unwind, which will have 'parent' set to
// nullptr
void __dios_unwind( struct _VM_Frame *stack, struct _VM_Frame *from, struct _VM_Frame *to )
    _PDCLIB_nothrow __attribute__((__noinline__));

// transfer control to given frame and program counter, if restoreMaskTo is -1
// it does not change mask
void __dios_jump( struct _VM_Frame *to, void (*pc)( void ), int restoreMaskTo )
    _PDCLIB_nothrow __attribute__((__noinline__));

// set value of register of instruction identified by 'pc' in frame identified
// by 'frame' 'lenght' bytes from 'data' will be written to offset 'offset' of
// the register
// function returns 1 if register was successfuly written and 0 otherwise
int __dios_set_register( struct _VM_Frame *frame, _VM_CodePointer pc,
                         unsigned offset, char *data, unsigned lenght );

// similar to __dios_set_register except it reads from given (part) of register
int __dios_get_register( struct _VM_Frame *frame, _VM_CodePointer pc,
                         unsigned offset, char *data, unsigned lenght );

_PDCLIB_EXTERN_END

#endif