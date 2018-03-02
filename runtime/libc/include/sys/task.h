// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Jan Mrázek <email@honzamrazek.cz>
 * (c) 2018 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __SYS_TASK_H__
#define __SYS_TASK_H__

#include <sys/cdefs.h>

struct __dios_tls
{
    int __errno;
    char __data[ 0 ];
};

typedef struct __dios_tls* __dios_task;
typedef void ( *__dios_task_routine )( void * );

__BEGIN_DECLS

/*
 * The resulting _DiOS_TaskHandle points to the beginning of TLS. Userspace
 * code is allowed to use it from offset _DiOS_TLS_Reserved.
 */
static inline __dios_task __dios_this_task() __nothrow
{
    return __CAST( __dios_task, __vm_ctl_get( _VM_CR_User2 ) );
}

/*
 * get a pointer to errno, which is in dios-managed task-local data (accessible
 * to userspace, but independent of pthreads)
 */
static inline int *__dios_errno() __nothrow
{
    return &__dios_this_task()->__errno;
}

/*
 * Start a new task and obtain its identifier. Task starts executing routine
 * with arg.
 * - tls_size is the total size of TLS, _DiOS_TLS_Reserved must be included in this,
 *   if tls_size is less then _DiOS_TLS_Reserved at least _DiOS_TLS_Reserved is allocated
 * - the resulting _DiOS_TaskHandle points to the beginning of TLS. Userspace is
 *   allowed to use it from offset _DiOS_TLS_Reserved
 */
__dios_task __dios_start_task( __dios_task_routine r, void *arg, int tls_size ) __nothrow;

void __dios_kill( __dios_task t ) __nothrow;

static inline void __dios_suicide() __nothrow
{
    __dios_kill( __dios_this_task() );
}

__dios_task *__dios_get_process_tasks() __nothrow;
void __dios_exit_process( int code ) __nothrow;
void __dios_yield() __nothrow;

__END_DECLS

#endif