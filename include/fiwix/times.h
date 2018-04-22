/*
 * fiwix/include/fiwix/times.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_TIMES_H
#define _FIWIX_TIMES_H

struct tms {
	__clock_t tms_utime;	/* CPU time spent in user-mode */
	__clock_t tms_stime;	/* CPU time spent in kernel-mode */
	__clock_t tms_cutime;	/* (tms_utime + tms_cutime) of children */
	__clock_t tms_cstime;	/* (tms_stime + tms_cstime) of children */
};

#endif /* _FIWIX_TIMES_H */
