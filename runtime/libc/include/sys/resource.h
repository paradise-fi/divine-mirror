/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)resource.h	8.2 (Berkeley) 1/4/94
 */


#ifndef _SYS_RESOURCE_H_
#define	_SYS_RESOURCE_H_

#include <sys/time.h>
#include <sys/types.h>
#include <_PDCLIB_aux.h>

/*
 * Process priority specifications to get/setpriority.
 */
#define	PRIO_MIN	(-20)
#define	PRIO_MAX	20

#define	PRIO_PROCESS	0
#define	PRIO_PGRP	1
#define	PRIO_USER	2

/*
 * Resource utilization information.
 */

#define	RUSAGE_SELF	0
#define	RUSAGE_CHILDREN	(-1)
#define	RUSAGE_THREAD	1

typedef __uint64_t rlim_t;
typedef __uint32_t id_t;

struct	rusage {
	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */
	long	ru_maxrss;		/* max resident set size */
#define	ru_first	ru_ixrss
	long	ru_ixrss;		/* integral shared text memory size */
	long	ru_idrss;		/* integral unshared data " */
	long	ru_isrss;		/* integral unshared stack " */
	long	ru_minflt;		/* page reclaims */
	long	ru_majflt;		/* page faults */
	long	ru_nswap;		/* swaps */
	long	ru_inblock;		/* block input operations */
	long	ru_oublock;		/* block output operations */
	long	ru_msgsnd;		/* messages sent */
	long	ru_msgrcv;		/* messages received */
	long	ru_nsignals;		/* signals received */
	long	ru_nvcsw;		/* voluntary context switches */
	long	ru_nivcsw;		/* involuntary " */
#define	ru_last		ru_nivcsw
};

/*
 * Resource limits
 */
#define RLIMIT_CPU      _HOST_RLIMIT_CPU      /* cpu time in milliseconds */
#define RLIMIT_FSIZE    _HOST_RLIMIT_FSIZE    /* maximum file size */
#define RLIMIT_DATA     _HOST_RLIMIT_DATA     /* data size */
#define RLIMIT_STACK    _HOST_RLIMIT_STACK    /* stack size */
#define RLIMIT_CORE     _HOST_RLIMIT_CORE     /* core file size */
#define RLIMIT_RSS      _HOST_RLIMIT_RSS      /* resident set size */
#define RLIMIT_MEMLOCK  _HOST_RLIMIT_MEMLOCK  /* locked-in-memory address space */
#define RLIMIT_NPROC    _HOST_RLIMIT_NPROC    /* number of processes */
#define RLIMIT_NOFILE   _HOST_RLIMIT_NOFILE   /* number of open files */

#define RLIM_NLIMITS    _HOST_RLIM_NLIMITS    /* number of resource limits */

#define	RLIM_INFINITY	(((rlim_t)1 << 63) - 1)
#define	RLIM_SAVED_MAX	RLIM_INFINITY
#define	RLIM_SAVED_CUR	RLIM_INFINITY

struct rlimit {
	rlim_t	rlim_cur;		/* current (soft) limit */
	rlim_t	rlim_max;		/* maximum value for rlim_cur */
};

#if __BSD_VISIBLE
/* Load average structure. */
struct loadavg {
	fixpt_t	ldavg[3];
	long	fscale;
};
#endif /* __BSD_VISIBLE */

_PDCLIB_EXTERN_C
int	getpriority(int, id_t);
int	getrlimit(int, struct rlimit *);
int	getrusage(int, struct rusage *);
int	setpriority(int, id_t, int);
int	setrlimit(int, const struct rlimit *);
_PDCLIB_EXTERN_END

#endif	/* !_SYS_RESOURCE_H_ */
