/*
 * fiwix/include/fiwix/unistd.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_UNISTD_H
#define _FIWIX_UNISTD_H

/*
 * This is intended to be pure Linux 2.0 ABI, plus some system calls from
 * Linux 2.2.
 */

/* #define SYS_setup */
#define SYS_exit		1
#define SYS_fork		2
#define SYS_read		3
#define SYS_write		4
#define SYS_open		5
#define SYS_close		6
#define SYS_waitpid		7
#define SYS_creat		8
#define SYS_link		9
#define SYS_unlink		10
#define SYS_execve		11
#define SYS_chdir		12
#define SYS_time		13
#define SYS_mknod		14
#define SYS_chmod		15
#define SYS_chown		16
#define SYS_break		17		/* -ENOSYS */
#define SYS_oldstat		18
#define SYS_lseek		19
#define SYS_getpid		20
#define SYS_mount		21
#define SYS_umount		22
#define SYS_setuid		23
#define SYS_getuid		24
#define SYS_stime		25
/* #define SYS_ptrace */
#define SYS_alarm		27
#define SYS_oldfstat		28
#define SYS_pause		29
#define SYS_utime		30
#define SYS_stty		31		/* -ENOSYS */
#define SYS_gtty		32		/* -ENOSYS */
#define SYS_access		33
/* #define SYS_nice */
#define SYS_ftime		35
#define SYS_sync		36
#define SYS_kill		37
#define SYS_rename		38
#define SYS_mkdir		39
#define SYS_rmdir		40
#define SYS_dup			41
#define SYS_pipe		42
#define SYS_times		43
#define SYS_prof		44		/* -ENOSYS */
#define SYS_brk			45
#define SYS_setgid		46
#define SYS_getgid		47
#define SYS_signal		48
#define SYS_geteuid		49
#define SYS_getegid		50
/* #define SYS_acct */
#define SYS_umount2		52	/* (from Linux 2.2) it was sys_phys() */
#define SYS_lock		53		/* -ENOSYS */
#define SYS_ioctl		54
#define SYS_fcntl		55
#define SYS_mpx			56		/* -ENOSYS */
#define SYS_setpgid		57
#define SYS_ulimit		58		/* -ENOSYS */
#define SYS_olduname		59
#define SYS_umask		60
#define SYS_chroot		61
#define SYS_ustat		62
#define SYS_dup2		63
#define SYS_getppid		64
#define SYS_getpgrp		65
#define SYS_setsid		66
#define SYS_sigaction		67
#define SYS_sgetmask		68
#define SYS_ssetmask		69
#define SYS_setreuid		70
#define SYS_setregid		71
#define SYS_sigsuspend		72
#define SYS_sigpending		73
#define SYS_sethostname		74
#define SYS_setrlimit		75
#define SYS_getrlimit		76
#define SYS_getrusage		77
#define SYS_gettimeofday	78
#define SYS_settimeofday	79
#define SYS_getgroups		80
#define SYS_setgroups		81
#define SYS_oldselect		82
#define SYS_symlink		83
#define SYS_oldlstat		84
#define SYS_readlink		85
/* #define SYS_uselib */
/* #define SYS_swapon */
#define SYS_reboot		88
/* #define SYS_oldreaddir */
#define SYS_old_mmap		90
#define SYS_munmap		91
#define SYS_truncate		92
#define SYS_ftruncate		93
#define SYS_fchmod		94
#define SYS_fchown		95
/* #define SYS_getpriority */
/* #define SYS_setpriority */
/* #define SYS_profil */
#define SYS_statfs		99
#define SYS_fstatfs		100
#define SYS_ioperm		101
#define SYS_socketcall 		102
/* #define SYS_syslog */
#define SYS_setitimer		104
#define SYS_getitimer		105
#define SYS_newstat		106
#define SYS_newlstat		107
#define SYS_newfstat		108
#define SYS_uname		109
#define SYS_iopl		110
/* #define SYS_vhangup */
/* #define SYS_idle		112		 -ENOSYS */
/* #define SYS_vm86old */
#define SYS_wait4		114
/* #define SYS_swapoff */
#define SYS_sysinfo		116
/* #define SYS_ipc */
#define SYS_fsync		118
#define SYS_sigreturn		119
/* #define SYS_clone */
#define SYS_setdomainname	121
#define SYS_newuname		122
/* #define SYS_modify_ldt */
/* #define SYS_adjtimex */
#define SYS_mprotect 		125
#define SYS_sigprocmask		126
/* #define SYS_create_module */
/* #define SYS_init_module */
/* #define SYS_delete_module */
/* #define SYS_get_kernel_syms */
/* #define SYS_quotactl */
#define SYS_getpgid 		132
#define SYS_fchdir		133
/* #define SYS_bdflush */
/* #define SYS_sysfs */
#define SYS_personality		136
/* #define afs_syscall */
#define SYS_setfsuid		138
#define SYS_setfsgid		139
#define SYS_llseek		140
#define SYS_getdents		141
#define SYS_select		142
#define SYS_flock		143
/* #define SYS_msync */
/* #define SYS_readv */
/* #define SYS_writev */
#define SYS_getsid		147
#define SYS_fdatasync		148
/* #define SYS_sysctl */
/* #define SYS_mlock */
/* #define SYS_munlock */
/* #define SYS_mlockall */
/* #define SYS_munlockall */
/* #define SYS_sched_setparam */
/* #define SYS_sched_getparam */
/* #define SYS_sched_setscheduler */
/* #define SYS_sched_getscheduler */
/* #define SYS_sched_yield */
/* #define SYS_sched_get_priority_max */
/* #define SYS_sched_get_priority_min */
/* #define SYS_sched_rr_get_interval */
#define SYS_nanosleep		162
/* #define SYS_mremap */

/* extra system calls from Linux 2.2 */
/* #define SYS_setresuid */
/* #define SYS_getresuid */
/* #define SYS_ni_syscall */
/* #define SYS_query_module */
/* #define SYS_poll */
/* #define SYS_nfsservctl */
/* #define SYS_setresgid */
/* #define SYS_getresgid */
/* #define SYS_prctl */
/* #define SYS_rt_sigreturn_wrapper */
/* #define SYS_rt_sigaction */
/* #define SYS_rt_sigprocmask */
/* #define SYS_rt_sigpending */
/* #define SYS_rt_sigtimedwait */
/* #define SYS_rt_sigqueueinfo */
/* #define SYS_rt_sigsuspend_wrapper */
/* #define SYS_pread */
/* #define SYS_pwrite */
/* #define SYS_chown */
#define SYS_getcwd		183
/* #define SYS_capget */
/* #define SYS_capset */
/* #define SYS_sigaltstack_wrapper */
/* #define SYS_sendfile */
/* #define SYS_ni_syscall */
/* #define SYS_ni_syscall */
#define SYS_vfork		190

#endif /* _FIWIX_UNISTD_H */
