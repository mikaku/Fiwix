/*
 * fiwix/include/fiwix/sysrq.h
 *
 * Copyright 2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_SYSRQ_H
#define _FIWIX_SYSRQ_H

/* the key combination consists of Alt+SysRq and another key, defined below */
#define SYSRQ_STACK	0x00000001	/* 'l' -> stack backtrace */
#define SYSRQ_MEMORY	0x00000002	/* 'm' -> memory information */
#define SYSRQ_TASKS	0x00000004	/* 't' -> task list */
#define SYSRQ_UNDEF	0x80000000	/* Undefined operation */

void do_sysrq(int);

#endif /* _FIWIX_SYSRQ_H */
