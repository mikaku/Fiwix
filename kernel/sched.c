/*
 * fiwix/kernel/sched.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/const.h>
#include <fiwix/sched.h>
#include <fiwix/process.h>
#include <fiwix/segments.h>
#include <fiwix/timer.h>
#include <fiwix/pic.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

extern struct seg_desc gdt[NR_GDT_ENTRIES];
int need_resched = 0;

static void context_switch(struct proc *next)
{
	struct proc *prev;

	kstat.ctxt++;
	prev = current;
	set_tss(next);
	current = next;
	do_switch(&prev->tss.esp, &prev->tss.eip, next->tss.esp, next->tss.eip, next->tss.cr3, TSS);
}

void set_tss(struct proc *p)
{
	struct seg_desc *g;

	g = &gdt[TSS / sizeof(struct seg_desc)];

	g->sd_lobase = (unsigned int)p;
	g->sd_loflags = SD_TSSPRESENT;
	g->sd_hibase = (char)(((unsigned int)p) >> 24);
}

/* Round Robin algorithm */
void do_sched(void)
{
	int count;
	struct proc *p, *selected;

	/* let the current running process consume its time slice */
	if(!need_resched && current->state == PROC_RUNNING && current->cpu_count > 0) {
		return;
	}

	need_resched = 0;
	for(;;) {
		count = -1;
		selected = &proc_table[IDLE];

		FOR_EACH_PROCESS(p) {
			if(p->state == PROC_RUNNING && p->cpu_count > count) {
				count = p->cpu_count;
				selected = p;
			}
		}
		if(count) {
			break;
		}

		/* reassigns new quantum to all processes */
		FOR_EACH_PROCESS(p) {
			if(p->state) {
				p->cpu_count = p->priority;
			}
		}
	}
	if(current != selected) {
		context_switch(selected);
	}
}

void sched_init(void)
{
	get_system_time();

	/* this should be more unpredictable */
	kstat.random_seed = CURRENT_TIME;
}
