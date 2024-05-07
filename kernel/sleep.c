/*
 * fiwix/kernel/sleep.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
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

#define NR_BUCKETS		((NR_PROCS * 10) / 100)	/* 10% of NR_PROCS */
#define SLEEP_HASH(addr)	((addr) % (NR_BUCKETS))

struct proc *sleep_hash_table[NR_BUCKETS];
struct proc *proc_run_head;
static unsigned int area = 0;

void runnable(struct proc *p)
{
	unsigned int flags;

	if(p->state == PROC_RUNNING) {
		printk("WARNING: %s(): process with pid '%d' is already running!\n", __FUNCTION__, p->pid);
		return;
	}

	SAVE_FLAGS(flags); CLI();
	if(proc_run_head) {
		p->next_run = proc_run_head;
		proc_run_head->prev_run = p;
	}
	proc_run_head = p;
	p->state = PROC_RUNNING;
	RESTORE_FLAGS(flags);
}

void not_runnable(struct proc *p, int state)
{
	unsigned int flags;

	SAVE_FLAGS(flags); CLI();
	if(p->next_run) {
		p->next_run->prev_run = p->prev_run;
	}
	if(p->prev_run) {
		p->prev_run->next_run = p->next_run;
	}
	if(p == proc_run_head) {
		proc_run_head = p->next_run;
	}
	p->prev_run = p->next_run = NULL;
	p->state = state;
	RESTORE_FLAGS(flags);
}

int sleep(void *address, int state)
{
	unsigned int flags;
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
		(*h)->prev_sleep = (*h)->next_sleep = NULL;
	} else {
		current->prev_sleep = NULL;
		current->next_sleep = *h;
		(*h)->prev_sleep = current;
		*h = current;
	}
	current->sleep_address = address;
	if(state == PROC_UNINTERRUPTIBLE) {
		current->flags |= PF_NOTINTERRUPT;
	}
	not_runnable(current, PROC_SLEEPING);

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
	unsigned int flags;
	struct proc **h;
	int i;

	SAVE_FLAGS(flags); CLI();
	i = SLEEP_HASH((unsigned int)address);
	h = &sleep_hash_table[i];

	while(*h) {
		if((*h)->sleep_address == address) {
			(*h)->sleep_address = NULL;
			(*h)->cpu_count = (*h)->priority;
			(*h)->flags &= ~PF_NOTINTERRUPT;
			runnable(*h);
			need_resched = 1;
			if((*h)->next_sleep) {
				(*h)->next_sleep->prev_sleep = (*h)->prev_sleep;
			}
			if((*h)->prev_sleep) {
				(*h)->prev_sleep->next_sleep = (*h)->next_sleep;
			}
			if(h == &sleep_hash_table[i]) {	/* if it's the head */
				*h = (*h)->next_sleep;
				continue;
			}
		}
		if(*h) {
			h = &(*h)->next_sleep;
		}
	}
	RESTORE_FLAGS(flags);
}

void wakeup_proc(struct proc *p)
{
	unsigned int flags;
	struct proc **h;
	int i;

	if(p->state != PROC_SLEEPING && p->state != PROC_STOPPED) {
		return;
	}

	/* return if the process is not interruptible */
	if(p->flags & PF_NOTINTERRUPT) {
		return;
	}

	SAVE_FLAGS(flags); CLI();

	/* stopped processes don't have sleep address */
	if(p->sleep_address) {
		if(p->next_sleep) {
			p->next_sleep->prev_sleep = p->prev_sleep;
		}
		if(p->prev_sleep) {
			p->prev_sleep->next_sleep = p->next_sleep;
		}

		i = SLEEP_HASH((unsigned int)p->sleep_address);
		h = &sleep_hash_table[i];

		if(*h == p) {	/* if it's the head */
			*h = (*h)->next_sleep;
		}
	}
	p->sleep_address = NULL;
	p->cpu_count = p->priority;
	runnable(p);
	need_resched = 1;

	RESTORE_FLAGS(flags);
}

void lock_resource(struct resource *resource)
{
	unsigned int flags;

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
	unsigned int flags;

	SAVE_FLAGS(flags); CLI();
	resource->locked = 0;
	if(resource->wanted) {
		resource->wanted = 0;
		wakeup(resource);
	}
	RESTORE_FLAGS(flags);
}

int lock_area(unsigned int type)
{
	unsigned int flags;
	int retval;

	SAVE_FLAGS(flags); CLI();
	retval = area & type;
	area |= type;
	RESTORE_FLAGS(flags);

	return retval;
}

int unlock_area(unsigned int type)
{
	unsigned int flags;
	int retval;

	SAVE_FLAGS(flags); CLI();
	retval = area & type;
	area &= ~type;
	RESTORE_FLAGS(flags);

	return retval;
}

void sleep_init(void)
{
	proc_run_head = NULL;
	memset_b(sleep_hash_table, 0, sizeof(sleep_hash_table));
}
