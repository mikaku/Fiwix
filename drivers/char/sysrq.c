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

static void memory(void)
{
	char *buf;

	buf = (char *)kmalloc(PAGE_SIZE);
	data_proc_meminfo(buf, 0);
	printk("%s", buf);
	printk("\n");
	data_proc_buddyinfo(buf, 0);
	printk("%s", buf);
	printk("\n");
	kfree((unsigned int)buf);
}

static void proc_list(void)
{
	struct proc *p;

	printk("USER   PID   PPID    RSS S SLEEP_ADDR CMD\n");
	FOR_EACH_PROCESS(p) {
		printk("%d    %5d  %5d  %5d %s ",
			p->uid,
			p->pid,
			p->ppid ? p->ppid->pid : 0,
			p->rss << 2,
			pstate[p->state]);
		if(p->state == PROC_SLEEPING) {
			printk("0x%08x ", p->sleep_address);
		} else {
			printk("           ");
		}
		printk("%s\n", p->argv0);
		p = p->next;
	}

	printk("List of PIDs in running queue: ");
	FOR_EACH_PROCESS_RUNNING(p) {
		printk("%d ", p->pid);
		p = p->next_run;
	}
	printk("\n");
}

void sysrq(int op)
{
	switch(op) {
		case SYSRQ_STACK:
			printk("sysrq: Stack backtrace.\n");
			stack_backtrace();
			break;
		case SYSRQ_MEMORY:
			printk("sysrq: Memory information.\n");
			memory();
			break;
		case SYSRQ_TASKS:
			printk("sysrq: Task list.\n");
			proc_list();
			break;
		case SYSRQ_UNDEF:
			printk("sysrq: Undefined operation.\n");
			break;
	}
}
