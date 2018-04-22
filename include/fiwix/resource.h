/*
 * fiwix/include/fiwix/resource.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_RESOURCE_H
#define _FIWIX_RESOURCE_H

#include <fiwix/time.h>

#define RLIMIT_INFINITY	0x7FFFFFFF	/* value to indicate "no limit" */
#define RLIM_INFINITY	RLIMIT_INFINITY	/* traditional name */

#define RUSAGE_SELF	0		/* the calling process */
#define RUSAGE_CHILDREN	(-1)		/* all of its termin. child processes */

#define RLIMIT_CPU	0		/* per-process CPU limit (secs) */
#define RLIMIT_FSIZE	1		/* largest file that can be created
					   (bytes */
#define RLIMIT_DATA	2		/* maximum size of data segment
					   (bytes) */
#define RLIMIT_STACK	3		/* maximum size of stack segment
					   (bytes) */
#define RLIMIT_CORE	4		/* largest core file that can be created
					   (bytes) */
#define RLIMIT_RSS	5		/* largest resident set size (bytes) */
#define RLIMIT_NPROC	6		/* number of processes */
#define RLIMIT_NOFILE	7		/* number of open files */
#define RLIMIT_MEMLOCK	8		/* locked-in-memory address space */
#define RLIMIT_AS	9		/* address space limit */

#define RLIM_NLIMITS	10

struct rusage {
	struct timeval ru_utime;	/* total amount of user time used */
	struct timeval ru_stime;	/* total amount of system time used */
	long ru_maxrss;			/* maximum resident set size (KB) */
	long ru_ixrss;			/* amount of sharing of text segment
					   memory with other processes
					   (KB-secs) */
	long ru_idrss;			/* amount of data segment memory used
					   (KB-secs) */
	long ru_isrss;			/* amount of stack memory used
					   (KB-secs) */
	long ru_minflt;			/* number of soft page faults (i.e.
					   those serviced by reclaiming a page
					   from the list of pages awaiting
					   rellocation) */
	long ru_majflt;			/* number of hard page faults (i.e.
					   those that required I/O) */
	long ru_nswap;			/* number of times a process was swapped
					   out of physical memory */
	long ru_inblock;		/* number of input operations via the
					   file system. Note this and
					   'ru_outblock' do not include
					   operations with the cache */
	long ru_oublock;		/* number of output operations via the
					   file system */
	long ru_msgsnd;			/* number of IPC messages sent */
	long ru_msgrcv;			/* number of IPC messages received */
	long ru_nsignals;		/* number of signals delivered */
	long ru_nvcsw;			/* number of voluntary context switches,
					   i.e. because the process gave up the
					   process before it had to (usually to
					   wait for some resouce to be
					   availabe */
	long ru_nivcsw;			/* number of involuntary context
					   switches. i.e. a higher priority
					   process became runnable or the
					   current process used up its time
					   slice */
};

struct rlimit {
	int rlim_cur;			/* the current (soft) limit */
	int rlim_max;			/* the maximum (hard) limit */
};

#endif /* _FIWIX_RESOURCE_H */
