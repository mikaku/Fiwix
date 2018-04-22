/*
 * fiwix/include/fiwix/sched.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_SCHED_H
#define _FIWIX_SCHED_H

#include <fiwix/process.h>

#define PRIO_PROCESS	0
#define PRIO_PGRP	1
#define PRIO_USER	2

#define PROC_UNUSED	0
#define PROC_RUNNING	1
#define PROC_SLEEPING	2
#define PROC_ZOMBIE	3
#define PROC_STOPPED	4
#define PROC_IDLE	5

#define PROC_INTERRUPTIBLE	1
#define PROC_UNINTERRUPTIBLE	2

#define DEF_PRIORITY	(20 * HZ / 100)	/* 200ms of time slice */

extern int need_resched;

#define SI_LOAD_SHIFT   16

/*
 * This was brougth from Linux 2.0.30 (sched.h).
 * Copyright Linus Torvalds et al.
 */
extern unsigned int avenrun[3];		/* Load averages */
#define FSHIFT		11		/* nr of bits of precision */
#define FIXED_1		(1<<FSHIFT)	/* 1.0 as fixed-point */
#define LOAD_FREQ	(5*HZ)		/* 5 sec intervals */
#define EXP_1		1884		/* 1/exp(5sec/1min) as fixed-point */
#define EXP_5		2014		/* 1/exp(5sec/5min) */
#define EXP_15		2037		/* 1/exp(5sec/15min) */

#define CALC_LOAD(load,exp,n) \
	load *= exp; \
	load += n*(FIXED_1-exp); \
	load >>= FSHIFT;
/* ------------------------------------------------------------------------ */


void do_sched(void);
void set_tss(struct proc *);
void sched_init(void);

#endif /* _FIWIX_SCHED_H */
