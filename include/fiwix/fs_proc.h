/*
 * fiwix/include/fiwix/fs_proc.h
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_FS_PROC_H
#define _FIWIX_FS_PROC_H

#include <fiwix/types.h>

#define PROC_ROOT_INO		1	/* root inode */
#define PROC_SUPER_MAGIC	0x9FA0	/* same as in Linux */

#define PROC_PID_INO		0x40000000	/* base for PID inodes */
#define PROC_PID_LEV		1	/* array level for PIDs */

#define PROC_FD_INO		0x50000000	/* base for FD inodes */
#define PROC_FD_LEV		2	/* array level for FDs */

#define PROC_ARRAY_ENTRIES	22

enum pid_dir_inodes {
	PROC_PID_FD = PROC_PID_INO + 1001,
	PROC_PID_CMDLINE,
	PROC_PID_CWD,
	PROC_PID_ENVIRON,
	PROC_PID_EXE,
	PROC_PID_MAPS,
	PROC_PID_MOUNTINFO,
	PROC_PID_ROOT,
	PROC_PID_STAT,
	PROC_PID_STATM,
	PROC_PID_STATUS
};

struct procfs_inode {
	unsigned int i_lev;		/* array level (directory depth) */
};

struct procfs_dir_entry {
	__ino_t inode;
	__mode_t mode;
	__nlink_t nlink;
	int lev;			/* array level (directory depth) */
	unsigned short int name_len;
	char *name;
	int (*data_fn)(char *, __pid_t);
};

extern struct procfs_dir_entry procfs_array[][PROC_ARRAY_ENTRIES + 1];

int data_proc_buddyinfo(char *, __pid_t);
int data_proc_cmdline(char *, __pid_t);
int data_proc_cpuinfo(char *, __pid_t);
int data_proc_devices(char *, __pid_t);
int data_proc_dma(char *, __pid_t);
int data_proc_filesystems(char *, __pid_t);
int data_proc_interrupts(char *, __pid_t);
int data_proc_loadavg(char *, __pid_t);
int data_proc_locks(char *, __pid_t);
int data_proc_meminfo(char *, __pid_t);
int data_proc_mounts(char *, __pid_t);
int data_proc_partitions(char *, __pid_t);
int data_proc_rtc(char *, __pid_t);
int data_proc_self(char *, __pid_t);
int data_proc_stat(char *, __pid_t);
int data_proc_uptime(char *, __pid_t);
int data_proc_fullversion(char *, __pid_t);
int data_proc_unix(char *, __pid_t);
int data_proc_buffernr(char *, __pid_t);
int data_proc_domainname(char *, __pid_t);
int data_proc_filemax(char *, __pid_t);
int data_proc_filenr(char *, __pid_t);
int data_proc_hostname(char *, __pid_t);
int data_proc_inodemax(char *, __pid_t);
int data_proc_inodenr(char *, __pid_t);
int data_proc_osrelease(char *, __pid_t);
int data_proc_ostype(char *, __pid_t);
int data_proc_version(char *, __pid_t);
int data_proc_dirty_background_ratio(char *, __pid_t);

/* PID related functions */
int data_proc_pid_fd(char *, __pid_t, __ino_t);
int data_proc_pid_cmdline(char *, __pid_t);
int data_proc_pid_cwd(char *, __pid_t);
int data_proc_pid_environ(char *, __pid_t);
int data_proc_pid_exe(char *, __pid_t);
int data_proc_pid_maps(char *, __pid_t);
int data_proc_pid_mountinfo(char *, __pid_t);
int data_proc_pid_root(char *, __pid_t);
int data_proc_pid_stat(char *, __pid_t);
int data_proc_pid_statm(char *, __pid_t);
int data_proc_pid_status(char *, __pid_t);

#endif /* _FIWIX_FS_PROC_H */
