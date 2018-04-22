/*
 * fiwix/include/fiwix/sleep.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_SLEEP_H
#define _FIWIX_SLEEP_H

#include <fiwix/process.h>

#define AREA_TTY_READ	0x00000001
#define AREA_CALLOUT	0x00000002

struct sleep {
	unsigned short int next;
	void *address;
	struct proc *proc;
};

struct resource {
	char locked;
	char wanted;
};

int sleep(void *, int);
void wakeup(void *);
void wakeup_proc(struct proc *);

void lock_resource(struct resource *);
void unlock_resource(struct resource *);
int lock_area(unsigned int);
int unlock_area(unsigned int);

void sleep_init(void);

#endif /* _FIWIX_SLEEP_H */
