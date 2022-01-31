/*
 * fiwix/kernel/syscalls/fork.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/segments.h>
#include <fiwix/sigcontext.h>
#include <fiwix/process.h>
#include <fiwix/sched.h>
#include <fiwix/sleep.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int sys_fork(int arg1, int arg2, int arg3, int arg4, int arg5, struct sigcontext *sc)
{
	int count, pages;
	unsigned int n;
	unsigned int *child_pgdir;
	struct sigcontext *stack;
	struct proc *child, *p;
	struct vma *vma;
	__pid_t pid;

#ifdef __DEBUG__
	printk("(pid %d) sys_fork()\n", current->pid);
#endif /*__DEBUG__ */

	/* check the number of processes already allocated by this UID */
	count = 0;
	FOR_EACH_PROCESS(p) {
		if(p->uid == current->uid) {
			count++;
		}
		p = p->next;
	}
	if(count > current->rlim[RLIMIT_NPROC].rlim_cur) {
		printk("WARNING: %s(): RLIMIT_NPROC exceeded.\n", __FUNCTION__);
		return -EAGAIN;
	}

	if(!(pid = get_unused_pid())) {
		return -EAGAIN;
	}
	if(!(child = get_proc_free())) {
		return -EAGAIN;
	}

	/* 
	 * This memcpy() will overwrite the prev and next pointers, so that's
	 * the reason why proc_slot_init() is separated from get_proc_free().
	 */
	memcpy_b(child, current, sizeof(struct proc));

	proc_slot_init(child);
	child->pid = pid;
	sprintk(child->pidstr, "%d", child->pid);

	if(!(child_pgdir = (void *)kmalloc())) {
		release_proc(child);
		return -ENOMEM;
	}
	child->rss++;
	memcpy_b(child_pgdir, kpage_dir, PAGE_SIZE);
	child->tss.cr3 = V2P((unsigned int)child_pgdir);

	child->ppid = current->pid;
	child->flags = 0;
	child->children = 0;
	child->cpu_count = child->priority;
	child->start_time = CURRENT_TICKS;
	child->sleep_address = NULL;

	memcpy_b(child->vma, current->vma, sizeof(child->vma));
	vma = child->vma;
	for(n = 0; n < VMA_REGIONS && vma->start; n++, vma++) {
		if(vma->inode) {
			vma->inode->count++;
		}
	}

	child->sigpending = 0;
	child->sigexecuting = 0;
	memset_b(&child->sc, NULL, sizeof(struct sigcontext));
	memset_b(&child->usage, NULL, sizeof(struct rusage));
	memset_b(&child->cusage, NULL, sizeof(struct rusage));
	child->it_real_interval = 0;
	child->it_real_value = 0;
	child->it_virt_interval = 0;
	child->it_virt_value = 0;
	child->it_prof_interval = 0;
	child->it_prof_value = 0;

	if(!(child->tss.esp0 = kmalloc())) {
		kfree((unsigned int)child_pgdir);
		kfree((unsigned int)child->vma);
		release_proc(child);
		return -ENOMEM;
	}

	if(!(pages = clone_pages(child))) {
		printk("WARNING: %s(): not enough memory, can't clone pages.\n", __FUNCTION__);
		free_page_tables(child);
		kfree((unsigned int)child_pgdir);
		kfree((unsigned int)child->vma);
		release_proc(child);
		return -ENOMEM;
	}
	child->rss += pages;
	invalidate_tlb();

	child->tss.esp0 += PAGE_SIZE - 4;
	child->rss++;
	child->tss.ss0 = KERNEL_DS;

	memcpy_b((unsigned int *)(child->tss.esp0 & PAGE_MASK), (void *)((unsigned int)(sc) & PAGE_MASK), PAGE_SIZE);
	stack = (struct sigcontext *)((child->tss.esp0 & PAGE_MASK) + ((unsigned int)(sc) & ~PAGE_MASK));

	child->tss.eip = (unsigned int)return_from_syscall;
	child->tss.esp = (unsigned int)stack;
	stack->eax = 0;		/* child returns 0 */

	/* increase file descriptors usage */
	for(n = 0; n < OPEN_MAX; n++) {
		if(current->fd[n]) {
			fd_table[current->fd[n]].count++;
		}
	}
	if(current->root) {
		current->root->count++;
	}
	if(current->pwd) {
		current->pwd->count++;
	}

	kstat.processes++;
	nr_processes++;
	current->children++;
	runnable(child);

	return child->pid;	/* parent returns child's PID */
}
