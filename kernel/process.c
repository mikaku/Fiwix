/*
 * fiwix/kernel/process.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/process.h>
#include <fiwix/timer.h>
#include <fiwix/sched.h>
#include <fiwix/sleep.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct proc *proc_table;
struct proc *current;

struct proc *proc_pool_head;
struct proc *proc_table_head;
struct proc *proc_table_tail;
unsigned int free_proc_slots = 0;

static struct resource slot_resource = { NULL, NULL };
static struct resource pid_resource = { NULL, NULL };

int nr_processes = 0;
__pid_t lastpid = 0;

int kill_pid(__pid_t pid, __sigset_t signum)
{
	struct proc *p;

	FOR_EACH_PROCESS(p) {
		if(p->pid == pid && (p->state && p->state != PROC_ZOMBIE)) {
			return send_sig(p, signum);
		}
	}
	return -ESRCH;
}

int kill_pgrp(__pid_t pgid, __sigset_t signum)
{
	struct proc *p;
	int found;

	found = 0;
	FOR_EACH_PROCESS(p) {
		if(p->pgid == pgid && (p->state && p->state != PROC_ZOMBIE)) {
			send_sig(p, signum);
			found = 1;
		}
	}

	if(!found) {
		return -ESRCH;
	}
	return 0;
}

/* sum up child (and its children) statistics */
void add_crusage(struct proc *p, struct rusage *cru)
{
	cru->ru_utime.tv_sec = p->usage.ru_utime.tv_sec + p->cusage.ru_utime.tv_sec;
	cru->ru_utime.tv_usec = p->usage.ru_utime.tv_usec + p->cusage.ru_utime.tv_usec;
	if(cru->ru_utime.tv_usec >= 1000000) {
		cru->ru_utime.tv_sec++;
		cru->ru_utime.tv_usec -= 1000000;
	}
	cru->ru_stime.tv_sec = p->usage.ru_stime.tv_sec + p->cusage.ru_stime.tv_sec;
	cru->ru_stime.tv_usec = p->usage.ru_stime.tv_usec + p->cusage.ru_stime.tv_usec;
	if(cru->ru_stime.tv_usec >= 1000000) {
		cru->ru_stime.tv_sec++;
		cru->ru_stime.tv_usec -= 1000000;
	}
	cru->ru_maxrss = p->usage.ru_maxrss + p->cusage.ru_maxrss;
	cru->ru_ixrss = p->usage.ru_ixrss + p->cusage.ru_ixrss;
	cru->ru_idrss = p->usage.ru_idrss + p->cusage.ru_idrss;
	cru->ru_isrss = p->usage.ru_isrss + p->cusage.ru_isrss;
	cru->ru_minflt = p->usage.ru_minflt + p->cusage.ru_minflt;
	cru->ru_majflt = p->usage.ru_majflt + p->cusage.ru_majflt;
	cru->ru_nswap = p->usage.ru_nswap + p->cusage.ru_nswap;
	cru->ru_inblock = p->usage.ru_inblock + p->cusage.ru_inblock;
	cru->ru_oublock = p->usage.ru_oublock + p->cusage.ru_oublock;
	cru->ru_msgsnd = p->usage.ru_msgsnd + p->cusage.ru_msgsnd;
	cru->ru_msgrcv = p->usage.ru_msgrcv + p->cusage.ru_msgrcv;
	cru->ru_nsignals = p->usage.ru_nsignals + p->cusage.ru_nsignals;
	cru->ru_nvcsw = p->usage.ru_nvcsw + p->cusage.ru_nvcsw;
	cru->ru_nivcsw = p->usage.ru_nivcsw + p->cusage.ru_nivcsw;
}

void get_rusage(struct proc *p, struct rusage *ru)
{
	struct rusage cru;

	/*
	 * Conforms with SUSv3 which specifies that if SIGCHLD is being ignored
	 * then the child statistics should not be added to the values returned
	 * by RUSAGE_CHILDREN.
	 */
	if(current->sigaction[SIGCHLD - 1].sa_handler == SIG_IGN) {
		return;
	}

	add_crusage(p, &cru);
	memcpy_b(ru, &cru, sizeof(struct rusage));
}

/* add child statistics to parent */
void add_rusage(struct proc *p)
{
	struct rusage cru;

	add_crusage(p, &cru);
	current->cusage.ru_utime.tv_sec += cru.ru_utime.tv_sec;
	current->cusage.ru_utime.tv_usec += cru.ru_utime.tv_usec;
	if(current->cusage.ru_utime.tv_usec >= 1000000) {
		current->cusage.ru_utime.tv_sec++;
		current->cusage.ru_utime.tv_usec -= 1000000;
	}
	current->cusage.ru_stime.tv_sec += cru.ru_stime.tv_sec;
	current->cusage.ru_stime.tv_usec += cru.ru_stime.tv_usec;
	if(current->cusage.ru_stime.tv_usec >= 1000000) {
		current->cusage.ru_stime.tv_sec++;
		current->cusage.ru_stime.tv_usec -= 1000000;
	}
	current->cusage.ru_maxrss += cru.ru_maxrss;
	current->cusage.ru_ixrss += cru.ru_ixrss;
	current->cusage.ru_idrss += cru.ru_idrss;
	current->cusage.ru_isrss += cru.ru_isrss;
	current->cusage.ru_minflt += cru.ru_minflt;
	current->cusage.ru_majflt += cru.ru_majflt;
	current->cusage.ru_nswap += cru.ru_nswap;
	current->cusage.ru_inblock += cru.ru_inblock;
	current->cusage.ru_oublock += cru.ru_oublock;
	current->cusage.ru_msgsnd += cru.ru_msgsnd;
	current->cusage.ru_msgrcv += cru.ru_msgrcv;
	current->cusage.ru_nsignals += cru.ru_nsignals;
	current->cusage.ru_nvcsw += cru.ru_nvcsw;
	current->cusage.ru_nivcsw += cru.ru_nivcsw;
}

struct proc * get_next_zombie(struct proc *parent)
{
	struct proc *p;

	if(proc_table_head == NULL) {
		PANIC("process table is empty!\n");
	}

	FOR_EACH_PROCESS(p) {
		if(p->ppid == parent->pid && p->state == PROC_ZOMBIE) {
			return p;
		}
	}

	return NULL;
}

__pid_t remove_zombie(struct proc *p)
{
	__pid_t pid;

	pid = p->pid;
	kfree(p->tss.esp0);
	p->rss--;
	kfree(P2V(p->tss.cr3));
	p->rss--;
	release_proc(p);
	current->children--;
	return pid;
}

/*
 * An orphaned process group is a process group in which the parent of every
 * member is either itself a member of the group or is not a member of the
 * group's session.
 */
int is_orphaned_pgrp(__pid_t pgid)
{
	struct proc *p, *pp;
	int retval;

	retval = 0;
	lock_resource(&slot_resource);

	FOR_EACH_PROCESS(p) {
		if(p->pgid != pgid) {
			continue;
		}
		if(p->state && p->state != PROC_ZOMBIE) {
			pp = get_proc_by_pid(p->ppid);
			/* return if only one is found that breaks the rule */
			if(pp->pgid != pgid || pp->sid == p->sid) {
				break;
			}
		}
	}

	unlock_resource(&slot_resource);
	return retval;
}

struct proc * get_proc_free(void)
{
	struct proc *p = NULL;

	if(free_proc_slots <= SAFE_SLOTS && !IS_SUPERUSER) {
		printk("WARNING: %s(): the remaining slots are only for root user!\n", __FUNCTION__);
		return NULL;
	}

	lock_resource(&slot_resource);

	if(proc_pool_head) {

		/* get (remove) a process slot from the free list */
		p = proc_pool_head;
		proc_pool_head = proc_pool_head->next;

		free_proc_slots--;
	} else {
		printk("WARNING: %s(): no more slots free in proc table!\n", __FUNCTION__);
	}

	unlock_resource(&slot_resource);
	return p;
}

void release_proc(struct proc *p)
{
	lock_resource(&slot_resource);

	/* remove a process from the proc_table */
	if(p == proc_table_tail) {
		if(proc_table_head == proc_table_tail) {
			proc_table_head = proc_table_tail = NULL;
		} else {
			proc_table_tail = proc_table_tail->prev;
			proc_table_tail->next = NULL;
		}
	} else {
		p->prev->next = p->next;
		p->next->prev = p->prev;
	}

	/* initialize and put a process slot back in the free list */
	memset_b(p, NULL, sizeof(struct proc));
	p->next = proc_pool_head;
	proc_pool_head = p;
	free_proc_slots++;

	unlock_resource(&slot_resource);
}

int get_unused_pid(void)
{
	short int loop;
	struct proc *p;

	loop = 0;
	lock_resource(&pid_resource);

loop:
	lastpid++;
	if(lastpid > MAX_PID_VALUE) {
		loop++;
		lastpid = INIT;
	}
	if(loop > 1) {
		printk("WARNING: %s(): system ran out of PID numbers!\n");
		return 0;
	}
	FOR_EACH_PROCESS(p) {
		if(p->state != PROC_UNUSED) {
			/*
			 * Make sure the kernel never reuses active pid, pgid
			 * or sid values.
			 */
			if(lastpid == p->pid || lastpid == p->pgid || lastpid == p->sid) {
				goto loop;
			}
		}
	}

	unlock_resource(&pid_resource);
	return lastpid;
}

struct proc * get_proc_by_pid(__pid_t pid)
{
	struct proc *p;

	FOR_EACH_PROCESS(p) {
		if(p->state && p->pid == pid) {
			return p;
		}
	}

	return NULL;
}

int get_new_user_fd(int fd)
{
	int n;

	for(n = fd; n < OPEN_MAX && n < current->rlim[RLIMIT_NOFILE].rlim_cur; n++) {
		if(current->fd[n] == 0) {
			current->fd[n] = -1;
			current->fd_flags[n] = 0;
			return n;
		}
	}

	return -EMFILE;
}

void release_user_fd(int ufd)
{
	current->fd[ufd] = 0;
}

struct proc * kernel_process(int (*fn)(void))
{
	struct proc *p;

	p = get_proc_free();
	proc_slot_init(p);
	p->pid = get_unused_pid();
	p->ppid = 0;
	p->flags |= PF_KPROC;
	p->priority = DEF_PRIORITY;
	if(!(p->tss.esp0 = kmalloc())) {
		release_proc(p);
		return NULL;
	}
	p->tss.esp0 += PAGE_SIZE - 4;
	p->rss++;
	p->tss.cr3 = (unsigned int)kpage_dir;
	p->tss.eip = (unsigned int)fn;
	p->tss.esp = p->tss.esp0;
	sprintk(p->pidstr, "%d", p->pid);
	p->state = PROC_RUNNING;
	return p;
}

void proc_slot_init(struct proc *p)
{
	int n;

	/* insert process at the end of proc_table */
	lock_resource(&slot_resource);
	if(proc_table_head == NULL) {
		p->prev = NULL;
		p->next = NULL;
		proc_table_head = proc_table_tail = p;
	} else {
		p->prev = proc_table_tail;
		p->next = NULL;
		proc_table_tail->next = p;
		proc_table_tail = p;
	}
	p->sleep_prev = p->sleep_next = NULL;
	unlock_resource(&slot_resource);

	memset_b(&p->tss, NULL, sizeof(struct i386tss));
	p->tss.io_bitmap_addr = (unsigned int)&p->io_bitmap;

	/*
	 * At the moment, all io_bitmap bits are setup to 0, which means full
	 * access. This must be changed to 1 once we have fixed the ioperm()
	 * system call.
	 */
	for(n = 0; n < IO_BITMAP_SIZE + 1; n++) {
		p->io_bitmap[n] = 0;	/* FIXME: change it to 1 */
	}
	p->state = PROC_IDLE;
}

void proc_init(void)
{
	int n;
	struct proc *p;

	memset_b(proc_table, NULL, proc_table_size);

	/* free list initialization */
	proc_pool_head = NULL;
	n = (proc_table_size / sizeof(struct proc)) - 1;
	do {
		p = &proc_table[n];

		/* fill the free list */
		p->next = proc_pool_head;
		proc_pool_head = p;
		free_proc_slots++;
	} while(n--);
	proc_table_head = proc_table_tail = NULL;
}
