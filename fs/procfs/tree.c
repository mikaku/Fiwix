/*
 * fiwix/fs/procfs/tree.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/stat.h>
#include <fiwix/fs.h>
#include <fiwix/fs_proc.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define DIR	S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | \
		S_IXOTH					/* dr-xr-xr-x */
#define REG	S_IFREG | S_IRUSR | S_IRGRP | S_IROTH	/* -r--r--r-- */
#define REGUSR	S_IFREG | S_IRUSR			/* -r-------- */
#define LNK	S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO	/* lrwxrwxrwx */
#define LNKPID	S_IFLNK | S_IRWXU			/* lrwx------ */

/*
 * WARNING: every time a new entry is added to this array you must also change
 * the PROC_ARRAY_ENTRIES value defined in fs_proc.h.
 */
struct procfs_dir_entry procfs_array[][PROC_ARRAY_ENTRIES + 1] = {
   {	/* [0] / */
	{ 1,     DIR,  2, 0, 1,  ".",            NULL },
	{ 2,     DIR,  2, 0, 2,  "..",           NULL },
	{ 3,     DIR,  3, 3, 3,  "sys",          NULL },
	{ 4,     REG,  1, 0, 7,  "cmdline",      data_proc_cmdline },
	{ 5,     REG,  1, 0, 7,  "cpuinfo",      data_proc_cpuinfo },
	{ 6,     REG,  1, 0, 7,  "devices",      data_proc_devices },
	{ 7,     REG,  1, 0, 3,  "dma",	         data_proc_dma },
	{ 8,     REG,  1, 0, 11, "filesystems",  data_proc_filesystems },
	{ 9,     REG,  1, 0, 10, "interrupts",   data_proc_interrupts },
	{ 10,    REG,  1, 0, 7,  "loadavg",      data_proc_loadavg },
	{ 11,    REG,  1, 0, 5,  "locks",        data_proc_locks },
	{ 12,    REG,  1, 0, 7,  "meminfo",      data_proc_meminfo },
	{ 13,    REG,  1, 0, 6,  "mounts",       data_proc_mounts },
	{ 14,    REG,  1, 0, 10, "partitions",   data_proc_partitions },
	{ 15,    REG,  1, 0, 3,  "rtc",          data_proc_rtc },
	{ 16,    LNK,  1, 0, 4,  "self",         data_proc_self },
	{ 17,    REG,  1, 0, 4,  "stat",         data_proc_stat },
	{ 18,    REG,  1, 0, 6,  "uptime",       data_proc_uptime },
	{ 19,    REG,  1, 0, 7,  "version",      data_proc_fullversion },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },
   {	/* [1] /PID/ */
	{ 1000,  DIR,  2, 1, 1,  ".",            NULL },
	{ 1,     DIR,  2, 0, 2,  "..",           NULL },
/*	{ PROC_PID_FD,      DIR,    2, 2, 2,  "fd",       data_proc_pid_fd },*/
	{ PROC_PID_CMDLINE, REG,    1, 1, 7,  "cmdline",  data_proc_pid_cmdline },
	{ PROC_PID_CWD,     LNKPID, 1, 1, 3,  "cwd",      data_proc_pid_cwd },
	{ PROC_PID_ENVIRON, REGUSR, 1, 1, 7,  "environ",  data_proc_pid_environ },
	{ PROC_PID_EXE,     LNKPID, 1, 1, 3,  "exe",      data_proc_pid_exe },
	{ PROC_PID_MAPS,    REG,    1, 1, 4,  "maps",     data_proc_pid_maps },
	{ PROC_PID_MOUNTINFO,REG,   1, 1, 9,  "mountinfo",data_proc_pid_mountinfo },
	{ PROC_PID_ROOT,    LNKPID, 1, 1, 4,  "root",     data_proc_pid_root },
	{ PROC_PID_STAT,    REG,    1, 1, 4,  "stat",     data_proc_pid_stat },
	{ PROC_PID_STATM,   REG,    1, 1, 5,  "statm",    data_proc_pid_statm },
	{ PROC_PID_STATUS,  REG,    1, 1, 6,  "status",   data_proc_pid_status },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },

   {
   },

   {	/* [3] /sys/ */
	{ 3,     DIR,  2, 3, 1,  ".",            NULL },
	{ 1,     DIR,  2, 0, 2,  "..",           NULL },
	{ 2001,  DIR,  2, 4, 6,  "kernel",       NULL },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },
   {	/* [4] /sys/kernel/ */
	{ 2001,  DIR,  2, 4, 1,  ".",            NULL },
	{ 3,     DIR,  2, 3, 2,  "..",           NULL },
	{ 3001,  REG,  1, 4, 10, "domainname",   data_proc_domainname },
	{ 3002,  REG,  1, 4, 8,  "file-max",     data_proc_filemax },
	{ 3003,  REG,  1, 4, 7,  "file-nr",      data_proc_filenr },
	{ 3004,  REG,  1, 4, 8,  "hostname",     data_proc_hostname },
	{ 3005,  REG,  1, 4, 9,  "inode-max",    data_proc_inodemax },
	{ 3006,  REG,  1, 4, 8,  "inode-nr",     data_proc_inodenr },
	{ 3007,  REG,  1, 4, 9,  "osrelease",    data_proc_osrelease },
	{ 3008,  REG,  1, 4, 6,  "ostype",       data_proc_ostype },
	{ 3009,  REG,  1, 4, 7,  "version",      data_proc_version },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   }
};

struct procfs_dir_entry * get_procfs_by_inode(struct inode *i)
{
	__ino_t inode;
	int n, lev;
	struct procfs_dir_entry *d;

	inode = i->inode;
	for(lev = 0; procfs_array[lev]; lev++) {
		if(lev == PROC_PID_LEV) {	/* PID entries */
			if((i->inode & 0xF0000000) == PROC_PID_INO) {
				inode = i->inode & 0xF0000FFF;
			}
		}
		d = procfs_array[lev];
		for(n = 0; n < PROC_ARRAY_ENTRIES && d->inode; n++) {
			if(d->inode == inode) {
				return d;
			}
			d++;
		}
	}

	return NULL;
}
