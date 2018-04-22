/*
 * fiwix/kernel/init.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/system.h>
#include <fiwix/mm.h>
#include <fiwix/timer.h>
#include <fiwix/sched.h>
#include <fiwix/fcntl.h>
#include <fiwix/stat.h>
#include <fiwix/process.h>
#include <fiwix/syscalls.h>
#include <fiwix/unistd.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define INIT_TRAMPOLINE_SIZE	128	/* max. size of init_trampoline() */

char *init_argv[] = { "init", NULL };
char *init_envp[] = { "HOME=/", "TERM=linux", NULL };

static void init_trampoline(void)
{
	USER_SYSCALL(SYS_open, "/dev/console", O_RDWR, 0);	/* stdin */
	USER_SYSCALL(SYS_dup, 0, NULL, NULL);			/* stdout */
	USER_SYSCALL(SYS_dup, 0, NULL, NULL);			/* stderr */
	USER_SYSCALL(SYS_execve, INIT_PROGRAM, init_argv, init_envp);

	/* only reached in case of error in sys_execve() */
	USER_SYSCALL(SYS_exit, NULL, NULL, NULL);
}

void init_init(void)
{
	int n;
	unsigned int page;
	struct inode *i;
	unsigned int *pgdir;
	struct proc *init;

	if(namei(INIT_PROGRAM, &i, NULL, FOLLOW_LINKS)) {
		PANIC("can't find %s.\n", INIT_PROGRAM);
	}
	if(!S_ISREG(i->i_mode)) {
		PANIC("%s is not a regular file.\n", INIT_PROGRAM);
	}
	iput(i);

	/* INIT slot was already created in main.c */
	init = &proc_table[INIT];

	/* INIT process starts with the current (kernel) Page Directory */
	if(!(pgdir = (void *)kmalloc())) {
		goto init_init__die;
	}
	init->rss++;
	memcpy_b(pgdir, kpage_dir, PAGE_SIZE);
	init->tss.cr3 = V2P((unsigned int)pgdir);

	if(!(init->vma = (void *)kmalloc())) {
		goto init_init__die;
	}
	init->rss++;
	memset_b(init->vma, NULL, PAGE_SIZE);

	init->ppid = 0;
	init->pgid = 0;
	init->sid = 0;
	init->flags = 0;
	init->children = 0;
	init->priority = DEF_PRIORITY;
	init->start_time = CURRENT_TIME;
	init->sleep_address = NULL;
	init->uid = init->gid = 0;
	init->euid = init->egid = 0;
	init->suid = init->sgid = 0;
	memset_b(init->fd, NULL, sizeof(init->fd));
	memset_b(init->fd_flags, NULL, sizeof(init->fd_flags));
	init->root = current->root;
	init->pwd = current->pwd;
	strcpy(init->argv0, init_argv[0]);
	sprintk(init->pidstr, "%d", init->pid);
	init->sigpending = 0;
	init->sigblocked = 0;
	init->sigexecuting = 0;
	memset_b(init->sigaction, NULL, sizeof(init->sigaction));
	memset_b(&init->usage, NULL, sizeof(struct rusage));
	memset_b(&init->cusage, NULL, sizeof(struct rusage));
	init->timeout = 0;
	for(n = 0; n < RLIM_NLIMITS; n++) {
		init->rlim[n].rlim_cur = init->rlim[n].rlim_max = RLIM_INFINITY;
	}
	init->rlim[RLIMIT_NOFILE].rlim_cur = OPEN_MAX;
	init->rlim[RLIMIT_NOFILE].rlim_max = NR_OPENS;
	init->rlim[RLIMIT_NPROC].rlim_cur = CHILD_MAX;
	init->rlim[RLIMIT_NPROC].rlim_cur = NR_PROCS;
	init->umask = 0022;

	/* setup the stack */
	if(!(init->tss.esp0 = kmalloc())) {
		goto init_init__die;
	}
	init->tss.esp0 += PAGE_SIZE - 4;
	init->rss++;
	init->tss.ss0 = KERNEL_DS;

	/* setup the init_trampoline */
	page = map_page(init, KERNEL_BASE_ADDR - PAGE_SIZE, 0, PROT_READ | PROT_WRITE);
	memcpy_b((void *)page, init_trampoline, INIT_TRAMPOLINE_SIZE);

	init->tss.eip = (unsigned int)switch_to_user_mode;
	init->tss.esp = page + PAGE_SIZE - 4;

  	init->state = PROC_RUNNING;
	nr_processes++;
	return;

init_init__die:
	PANIC("unable to run init process.\n");
}
