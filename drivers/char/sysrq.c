/*
 * fiwix/drivers/char/sysrq.c
 *
 * Copyright 2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/sysrq.h>
#include <fiwix/traps.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/mm.h>

static const char *pstate[] = {
	"?",
	"R",
	"S",
	"Z",
	"T",
	"D"
};

static void process_list(void)
{
	struct proc *p;

	printk("USER   PID   PPID  S  CMD\n");
	FOR_EACH_PROCESS(p) {
		if(p->state != PROC_ZOMBIE) {
			printk("%d    %5d  %5d  %s  %s\n",
				p->uid,
				p->pid,
				p->ppid,
				pstate[p->state],
				p->argv0
			);
		}
		p = p->next;
	}
}

void do_sysrq(int op)
{
	switch(op) {
		case SYSRQ_STACK:
			printk("sysrq: Stack backtrace.\n");
			stack_backtrace();
			break;
		case SYSRQ_TASKS:
			printk("sysrq: Task list.\n");
			process_list();
			break;
		case SYSRQ_UNDEF:
			printk("sysrq: Undefined operation.\n");
			break;
	}
}
