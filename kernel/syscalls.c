/*
 * fiwix/kernel/syscalls.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/syscalls.h>
#include <fiwix/mm.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#endif /*__DEBUG__ */

static int verify_address(int type, const void *addr, unsigned int size)
{
#ifdef CONFIG_LAZY_USER_ADDR_CHECK
	if(!addr) {
		return -EFAULT;
	}
#else
	struct vma *vma;
	unsigned int start;

	/*
	 * The vma_table of the INIT process is not setup yet when it
	 * calls sys_open() and sys_execve() from init_trampoline(),
	 * but these calls are trusted.
	 */
	if(!current->vma_table) {
		return 0;
	}

	start = (unsigned int)addr;
	if(!(vma = find_vma_region(start))) {
		/*
		 * We need to check here if addr looks like a possible
		 * non-existent user stack address. If so, just return 0
		 * and let 'do_page_fault()' to handle the imminent page
		 * fault as soon as the kernel will try to access it.
		 */
		vma = current->vma_table->prev;
		if(vma) {
			if(vma->s_type == P_STACK) {
				if(start < vma->start && start > vma->prev->end) {
					return 0;
				}
			}
		}
		return -EFAULT;
	}

	for(;;) {
		if(type == VERIFY_WRITE) {
			if(!(vma->prot & PROT_WRITE)) {
				return -EFAULT;
			}
		} else {
			if(!(vma->prot & PROT_READ)) {
				return -EFAULT;
			}
		}
		if(start + size < vma->end) {
			break;
		}
		if(!(vma = find_vma_region(vma->end))) {
			return -EFAULT;
		}
	}
#endif /* CONFIG_LAZY_USER_ADDR_CHECK */

	return 0;
}

void free_name(const char *name)
{
	kfree((unsigned int)name);
}

/*
 * This function has two objectives:
 *
 * 1. verifies the memory address validity of the char pointer supplied by the
 *    user and, at the same time, limits its length to PAGE_SIZE (4096) bytes.
 * 2. creates a copy of 'string' in the kernel data space.
 */
int malloc_name(const char *string, char **name)
{
	char *b;
	int n, errno;

	if((errno = verify_address(PROT_READ, string, 0))) {
		return errno;
	}

	if(!(b = (char *)kmalloc())) {
		return -ENOMEM;
	}
	*name = b;
	for(n = 0; n < PAGE_SIZE; n++) {
		if(!(*b = *string)) {
			return 0;
		}
		b++;
		string++;
	}

	free_name(*name);
	return -ENAMETOOLONG;
}

int check_user_permission(struct inode *i)
{
	if(!IS_SUPERUSER) {
		if(current->euid != i->i_uid) {
			return 1;
		}
	}
	return 0;
}

int check_group(struct inode *i)
{
	int n;
	__gid_t gid;

	if(current->flags & PF_USEREAL) {
		gid = current->gid;
	} else {
		gid = current->egid;
	}

	if(i->i_gid == gid) {
		return 0;
	}

	for(n = 0; n < NGROUPS_MAX; n++) {
		if(current->groups[n] == -1) {
			break;
		}
		if(current->groups[n] == i->i_gid) {
			return 0;
		}
	}
	return 1;
}

int check_user_area(int type, const void *addr, unsigned int size)
{
	return verify_address(type, addr, size);
}

int check_permission(int mask, struct inode *i)
{
	__uid_t uid;

	if(current->flags & PF_USEREAL) {
		uid = current->uid;
	} else {
		uid = current->euid;
	}

	if(mask & TO_EXEC) {
		if(!(i->i_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
			return -EACCES;
		}
	}
	if(uid == 0) {
		return 0;
	}
	if(i->i_uid == uid) {
		if((((i->i_mode >> 6) & 7) & mask) == mask) {
			return 0;
		}
	}
	if(!check_group(i)) {
		if((((i->i_mode >> 3) & 7) & mask) == mask) {
			return 0;
		}
	}
	if(((i->i_mode & 7) & mask) == mask) {
		return 0;
	}

	return -EACCES;
}


/* Linux 2.0 ABI system call (plus some from Linux 2.2) */
void *syscall_table[] = {
	NULL,				/* 0 */	/* sys_setup (-ENOSYS) */
	sys_exit,
	sys_fork,
	sys_read,
	sys_write,
	sys_open,			/* 5 */
	sys_close,
	sys_waitpid,
	sys_creat,
	sys_link,
	sys_unlink,			/* 10 */
	sys_execve,
	sys_chdir,
	sys_time,
	sys_mknod,
	sys_chmod,			/* 15 */
	sys_chown,
	NULL,					/* sys_break (-ENOSYS) */
	sys_stat,
	sys_lseek,
	sys_getpid,			/* 20 */
	sys_mount,
	sys_umount,
	sys_setuid,
	sys_getuid,
	sys_stime, 			/* 25 */
	NULL,	/* sys_ptrace */
	sys_alarm,
	sys_fstat,
	sys_pause,
	sys_utime,			/* 30 */
	NULL,					/* sys_stty (-ENOSYS) */
	NULL,					/* sys_gtty (-ENOSYS) */
	sys_access,
	NULL,	/* sys_nice */
	sys_ftime,			/* 35 */
	sys_sync,
	sys_kill,
	sys_rename,
	sys_mkdir,
	sys_rmdir,			/* 40 */
	sys_dup,
	sys_pipe,
	sys_times,
	NULL,	/* sys_prof */
	sys_brk,			/* 45 */
	sys_setgid,
	sys_getgid,
	sys_signal,
	sys_geteuid,
	sys_getegid,			/* 50 */
	NULL,	/* sys_acct */
	sys_umount2,
	NULL,					/* sys_lock (-ENOSYS) */
	sys_ioctl,
	sys_fcntl,			/* 55 */
	NULL,					/* sys_mpx (-ENOSYS) */
	sys_setpgid,
	NULL,					/* sys_ulimit (-ENOSYS) */
	sys_olduname,
	sys_umask,			/* 60 */
	sys_chroot,
	sys_ustat,
	sys_dup2,
	sys_getppid,
	sys_getpgrp,			/* 65 */
	sys_setsid,
	sys_sigaction,
	sys_sgetmask,
	sys_ssetmask,
	sys_setreuid,			/* 70 */
	sys_setregid,
	sys_sigsuspend,
	sys_sigpending,
	sys_sethostname,
	sys_setrlimit,			/* 75 */
	sys_getrlimit,
	sys_getrusage,
	sys_gettimeofday,
	sys_settimeofday,
	sys_getgroups,			/* 80 */
	sys_setgroups,
	old_select,
	sys_symlink,
	sys_lstat,
	sys_readlink,			/* 85 */
	NULL,	/* sys_uselib */
	NULL,	/* sys_swapon */
	sys_reboot,
	NULL,	/* old_readdir */
	old_mmap,			/* 90 */
	sys_munmap,
	sys_truncate,
	sys_ftruncate,
	sys_fchmod,
	sys_fchown,			/* 95 */
	NULL,	/* sys_getpriority */
	NULL,	/* sys_setpriority */
	NULL,					/* sys_profil (-ENOSYS) */
	sys_statfs,
	sys_fstatfs,			/* 100 */
	sys_ioperm,
	sys_socketcall,	/* sys_socketcall XXX */
	NULL,	/* sys_syslog */
	sys_setitimer,
	sys_getitimer,			/* 105 */
	sys_newstat,
	sys_newlstat,
	sys_newfstat,
	sys_uname,
	sys_iopl,			/* 110 */
	NULL,	/* sys_vhangup */
	NULL,					/* sys_idle (-ENOSYS) */
	NULL,	/* sys_vm86old */
	sys_wait4,
	NULL,	/* sys_swapoff */	/* 115 */
	sys_sysinfo,
#ifdef CONFIG_SYSVIPC
	sys_ipc,
#else
	NULL,	/* sys_ipc */
#endif /* CONFIG_SYSVIPC */
	sys_fsync,
	sys_sigreturn,
	NULL,	/* sys_clone */		/* 120 */
	sys_setdomainname,
	sys_newuname,
	NULL,	/* sys_modify_ldt */
	NULL,	/* sys_adjtimex */
	sys_mprotect,			/* 125 */
	sys_sigprocmask,
	NULL,	/* sys_create_module */
	NULL,	/* sys_init_module */
	NULL,	/* sys_delete_module */
	NULL,	/* sys_get_kernel_syms */	/* 130 */
	NULL,	/* sys_quotactl */
	sys_getpgid,
	sys_fchdir,
	NULL,	/* sys_bdflush */
	NULL,	/* sys_sysfs */		/* 135 */
	sys_personality,
	NULL,					/* afs_syscall (-ENOSYS) */
	sys_setfsuid,
	sys_setfsgid,
	sys_llseek,			/* 140 */
	sys_getdents,
	sys_select,
	sys_flock,
	NULL,	/* sys_msync */
	NULL,	/* sys_readv */		/* 145 */
	NULL,	/* sys_writev */
	sys_getsid,
	sys_fdatasync,
	NULL,	/* sys_sysctl */
	NULL,	/* sys_mlock */		/* 150 */
	NULL,	/* sys_munlock */
	NULL,	/* sys_mlockall */
	NULL,	/* sys_munlockall */
	NULL,	/* sys_sched_setparam */
	NULL,	/* sys_sched_getparam */	/* 155 */
	NULL,	/* sys_sched_setscheduler */
	NULL,	/* sys_sched_getscheduler */
	NULL,	/* sys_sched_yield */
	NULL,	/* sys_sched_get_priority_max */
	NULL,	/* sys_sched_get_priority_min */	/* 160 */
	NULL,	/* sys_sched_rr_get_interval */
	sys_nanosleep,
	NULL,	/* sys_mremap */
	NULL,
	NULL,				/* 165 */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,				/* 170 */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,				/* 175 */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,				/* 180 */
	NULL,
	sys_chown,
	sys_getcwd,
	NULL,
	NULL,				/* 185 */
	NULL,
	NULL,
	NULL,
	NULL,
	sys_fork,			/* 190 (sys_vfork) */
};

static void do_bad_syscall(unsigned int num)
{
#ifdef __DEBUG__
	printk("***** (pid %d) system call %d not supported yet *****\n", current->pid, num);
#endif /*__DEBUG__ */
}

/*
 * The argument 'struct sigcontext' is needed because there are some system
 * calls (such as sys_iopl and sys_fork) that need to get information from
 * certain registers (EFLAGS and ESP). The rest of system calls will ignore
 * such extra argument.
 */
#ifdef CONFIG_SYSCALL_6TH_ARG
int do_syscall(unsigned int num, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, struct sigcontext sc)
#else
int do_syscall(unsigned int num, int arg1, int arg2, int arg3, int arg4, int arg5, struct sigcontext sc)
#endif /* CONFIG_SYSCALL_6TH_ARG */
{
	int (*sys_func)(int, ...);

	if(num > NR_SYSCALLS) {
		do_bad_syscall(num);
		return -ENOSYS;
	}
	sys_func = syscall_table[num];
	if(!sys_func) {
		do_bad_syscall(num);
		return -ENOSYS;
	}
	current->sp = (unsigned int)&sc;
#ifdef CONFIG_SYSCALL_6TH_ARG
	return sys_func(arg1, arg2, arg3, arg4, arg5, arg6, &sc);
#else
	return sys_func(arg1, arg2, arg3, arg4, arg5, &sc);
#endif /* CONFIG_SYSCALL_6TH_ARG */
}
