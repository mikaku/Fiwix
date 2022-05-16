/*
 * fiwix/include/fiwix/process.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_PROCESS_H
#define _FIWIX_PROCESS_H

struct vma {
	unsigned int start;
	unsigned int end;
	char prot;		/* PROT_READ, PROT_WRITE, ... */
	unsigned int flags;	/* MAP_SHARED, MAP_PRIVATE, ... */
	unsigned int offset;
	char s_type;		/* section type (P_TEXT, P_DATA, ...) */
	struct inode *inode;	/* file inode */
	char o_mode;		/* open mode (O_RDONLY, O_RDWR, ...) */
};

#include <fiwix/config.h>
#include <fiwix/types.h>
#include <fiwix/signal.h>
#include <fiwix/limits.h>
#include <fiwix/sigcontext.h>
#include <fiwix/time.h>
#include <fiwix/resource.h>
#include <fiwix/tty.h>

#define IDLE		0		/* PID of idle */
#define INIT		1		/* PID of /sbin/init */
#define SAFE_SLOTS	2		/* process slots reserved for root */
#define SLOT(p)		((p) - (&proc_table[0]))

/* bits in flags */
#define PF_KPROC	0x00000001	/* kernel internal process */
#define PF_PEXEC	0x00000002	/* has performed a sys_execve() */
#define PF_USEREAL	0x00000004	/* use real UID in permission checks */

#define MMAP_START	0x40000000	/* mmap()s start at 1GB */
#define IS_SUPERUSER	(current->euid == 0)

#define IO_BITMAP_SIZE	8192		/* 8192*8bit = all I/O address space */

#define PG_LEADER(p)	((p)->pid == (p)->pgid)
#define SESS_LEADER(p)	((p)->pid == (p)->pgid && (p)->pid == (p)->sid)

#define FOR_EACH_PROCESS(p)		p = proc_table_head->next ; while(p)
#define FOR_EACH_PROCESS_RUNNING(p)	p = proc_run_head ; while(p)

/* value to be determined during system startup */
extern unsigned int proc_table_size;	/* size in bytes */

extern char any_key_to_reboot;
extern int nr_processes;
extern __pid_t lastpid;
extern struct proc *proc_table_head;

struct binargs {
	unsigned int page[ARG_MAX];
	int argc;
	int argv_len;
	int envc;
	int envp_len;
	int offset;
};

/* Intel 386 Task Switch State */
struct i386tss {
	unsigned int prev_tss;
	unsigned int esp0;
	unsigned int ss0;
	unsigned int esp1;
	unsigned int ss1;
	unsigned int esp2;
	unsigned int ss2;
	unsigned int cr3;
	unsigned int eip;
	unsigned int eflags;
	unsigned int eax;
	unsigned int ecx;
	unsigned int edx;
	unsigned int ebx;
	unsigned int esp;
	unsigned int ebp;
	unsigned int esi;
	unsigned int edi;
	unsigned int es;
	unsigned int cs;
	unsigned int ss;
	unsigned int ds;
	unsigned int fs;
	unsigned int gs;
	unsigned int ldt;
	unsigned short int debug_trap;
	unsigned short int io_bitmap_addr;
	unsigned char io_bitmap[IO_BITMAP_SIZE + 1];
};

struct proc {
	struct i386tss tss;
	__pid_t pid;			/* process ID */
	__pid_t ppid;			/* parent process ID */
	__pid_t pgid;			/* process group ID */
	__pid_t sid;			/* session ID */
	int flags;
	int groups[NGROUPS_MAX];
	int children;			/* number of children */
	struct tty *ctty;		/* controlling terminal */
	int state;			/* process state */
	int priority;
	int cpu_count;			/* time of process running */
	__time_t start_time;
	int exit_code;	
	void *sleep_address;
	unsigned short int uid;		/* real user ID */
	unsigned short int gid;		/* real group ID */
	unsigned short int euid;	/* effective user ID */
	unsigned short int egid;	/* effective group ID */
	unsigned short int suid;	/* saved user ID */
	unsigned short int sgid;	/* saved group ID */
	unsigned short int fd[OPEN_MAX];
	unsigned char fd_flags[OPEN_MAX];
	struct inode *root;
	struct inode *pwd;		/* process working directory */
	unsigned int entry_address;
	char argv0[NAME_MAX + 1];
	int argc;
	char **argv;
	int envc;
	char **envp;
	char pidstr[5];			/* PID number converted to string */
	struct vma vma[VMA_REGIONS];	/* virtual memory-map addresses */
	unsigned int brk_lower;		/* lower limit of the heap section */
	unsigned int brk;		/* current limit of the heap */
	__sigset_t sigpending;
	__sigset_t sigblocked;
	__sigset_t sigexecuting;
	struct sigaction sigaction[NSIG];
	struct sigcontext sc[NSIG];	/* each signal has its own context */
	unsigned int sp;		/* current process' stack frame */
	struct rusage usage;		/* process resource usage */
	struct rusage cusage;		/* children resource usage */
	unsigned long int it_real_interval, it_real_value;
	unsigned long int it_virt_interval, it_virt_value;
	unsigned long int it_prof_interval, it_prof_value;
	unsigned long int timeout;
	struct rlimit rlim[RLIM_NLIMITS];
	unsigned long int rss;
	__mode_t umask;
	unsigned char loopcnt;		/* nested symlinks counter */
	struct proc *prev;
	struct proc *next;
	struct proc *prev_sleep;
	struct proc *next_sleep;
	struct proc *prev_run;
	struct proc *next_run;
};

extern struct proc *current;
extern struct proc *proc_table;

int can_signal(struct proc *);
int send_sig(struct proc *, __sigset_t);

void add_crusage(struct proc *, struct rusage *);
void get_rusage(struct proc *, struct rusage *);
void add_rusage(struct proc *);
struct proc * get_next_zombie(struct proc *);
__pid_t remove_zombie(struct proc *);
int is_orphaned_pgrp(__pid_t);
struct proc * get_proc_free(void);
void release_proc(struct proc *);
int get_unused_pid(void);
struct proc * get_proc_by_pid(__pid_t);

int get_new_user_fd(int);
void release_user_fd(int);

struct proc * kernel_process(const char *, int (*fn)(void));
void proc_slot_init(struct proc *);
void proc_init(void);

int elf_load(struct inode *, struct binargs *, struct sigcontext *, char *);
int script_load(char *, char *, char *);

#endif /* _FIWIX_PROCESS_H */
