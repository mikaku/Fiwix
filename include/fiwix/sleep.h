/*
 * fiwix/include/fiwix/sleep.h
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_SLEEP_H
#define _FIWIX_SLEEP_H

#include <fiwix/process.h>

#define AREA_BH			0x00000001
#define AREA_CALLOUT		0x00000002
#define AREA_TTY_READ		0x00000004
#define AREA_SERIAL_READ	0x00000008

extern struct proc *run_head;

struct resource {
	char locked;
	char wanted;
};

void runnable(struct proc *);
void not_runnable(struct proc *, int);
int sleep(void *, int);
void wakeup(void *);
void wakeup_proc(struct proc *);

void lock_resource(struct resource *);
void unlock_resource(struct resource *);
int lock_area(unsigned int);
int unlock_area(unsigned int);

void sleep_init(void);

#endif /* _FIWIX_SLEEP_H */
