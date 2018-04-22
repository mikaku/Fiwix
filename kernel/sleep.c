/*
 * fiwix/kernel/sleep.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/limits.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/signal.h>
#include <fiwix/process.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define NR_BUCKETS		(NR_PROCS * 10) / 100	/* 10% of NR_PROCS */
#define SLEEP_HASH(addr)	((addr) % NR_BUCKETS)

struct proc *sleep_hash_table[NR_BUCKETS];
static unsigned int area = 0;

int sleep(void *address, int state)
{
	unsigned long int flags;
	struct proc **h;
	int signum, i;

	/* return if it has signals */
	if(state == PROC_INTERRUPTIBLE) {
		if((signum = issig())) {
			return signum;
		}
	}

	if(current->state == PROC_SLEEPING) {
		printk("WARNING: %s(): process with pid '%d' is already sleeping!\n", __FUNCTION__, current->pid);
		return 0;
	}

	SAVE_FLAGS(flags); CLI();
	i = SLEEP_HASH((unsigned int)address);
	h = &sleep_hash_table[i];

	/* insert process in the head */
	if(!*h) {
		*h = current;
		(*h)->sleep_prev = (*h)->sleep_next = NULL;
	} else {
		current->sleep_prev = NULL;
		current->sleep_next = *h;
		(*h)->sleep_prev = current;
		*h = current;
	}
	current->sleep_address = address;
	current->state = PROC_SLEEPING;

	do_sched();

	signum = 0;
	if(state == PROC_INTERRUPTIBLE) {
		signum = issig();
	}

	RESTORE_FLAGS(flags);
	return signum;
}

void wakeup(void *address)
{
	unsigned long int flags;
	struct proc **h;
	int i;

	SAVE_FLAGS(flags); CLI();
	i = SLEEP_HASH((unsigned int)address);
	h = &sleep_hash_table[i];

	while(*h) {
		if((*h)->sleep_address == address) {
			(*h)->sleep_address = NULL;
			(*h)->state = PROC_RUNNING;
			need_resched = 1;
			if((*h)->sleep_next) {
				(*h)->sleep_next->sleep_prev = (*h)->sleep_prev;
			}
			if((*h)->sleep_prev) {
				(*h)->sleep_prev->sleep_next = (*h)->sleep_next;
			}
			if(h == &sleep_hash_table[i]) {	/* if it's the head */
				*h = (*h)->sleep_next;
				continue;
			}
		}
		if(*h) {
			h = &(*h)->sleep_next;
		}
	}
	RESTORE_FLAGS(flags);
}

void wakeup_proc(struct proc *p)
{
	unsigned long int flags;
	struct proc **h;
	int i;

	if(p->state != PROC_SLEEPING && p->state != PROC_STOPPED) {
		return;
	}

	SAVE_FLAGS(flags); CLI();

	/* stopped processes don't have sleep address */
	if(p->sleep_address) {
		if(p->sleep_next) {
			p->sleep_next->sleep_prev = p->sleep_prev;
		}
		if(p->sleep_prev) {
			p->sleep_prev->sleep_next = p->sleep_next;
		}

		i = SLEEP_HASH((unsigned int)p->sleep_address);
		h = &sleep_hash_table[i];
		if(*h == p) {	/* if it's the head */
			*h = (*h)->sleep_next;
		}
	}
	p->sleep_address = NULL;
	p->state = PROC_RUNNING;
	need_resched = 1;

	RESTORE_FLAGS(flags);
	return;
}

void lock_resource(struct resource *resource)
{
	unsigned long int flags;

	for(;;) {
		SAVE_FLAGS(flags); CLI();
		if(resource->locked) {
			resource->wanted = 1;
			RESTORE_FLAGS(flags);
			sleep(resource, PROC_UNINTERRUPTIBLE);
		} else {
			break;
		}
	}
	resource->locked = 1;
	RESTORE_FLAGS(flags);
}

void unlock_resource(struct resource *resource)
{
	unsigned long int flags;

	SAVE_FLAGS(flags); CLI();
	resource->locked = 0;
	if(resource->wanted) {
		resource->wanted = 0;
		wakeup(resource);
	}
	RESTORE_FLAGS(flags);
}

int lock_area(unsigned int flag)
{
	unsigned long int flags;
	int retval;

	SAVE_FLAGS(flags); CLI();
	retval = area & flag;
	area |= flag;
	RESTORE_FLAGS(flags);

	return retval;
}

int unlock_area(unsigned int flag)
{
	unsigned long int flags;
	int retval;

	SAVE_FLAGS(flags); CLI();
	retval = area & flag;
	area &= ~flag;
	RESTORE_FLAGS(flags);

	return retval;
}

void sleep_init(void)
{
	memset_b(sleep_hash_table, NULL, sizeof(sleep_hash_table));
}
