/*
 * fiwix/fs/procfs/tree.c
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
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
#define DIRFD	S_IFDIR | S_IRUSR | S_IXUSR		/* dr-x------ */
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
	{ 1,     DIR,    2, 0, 1,  ".",   NULL },
	{ 2,     DIR,    2, 0, 2,  "..",  NULL },
	{ 3,     DIR,    2, 3, 3,  "net", NULL },
	{ 4,     DIR,    4, 4, 3,  "sys", NULL },
	{ 5,             REG,    1, 0, 9,  "buddyinfo",  data_proc_buddyinfo },
	{ 6,             REG,    1, 0, 7,  "cmdline",    data_proc_cmdline },
	{ 7,             REG,    1, 0, 7,  "cpuinfo",    data_proc_cpuinfo },
	{ 8,             REG,    1, 0, 7,  "devices",    data_proc_devices },
	{ 9,             REG,    1, 0, 3,  "dma",        data_proc_dma },
	{ 10,            REG,    1, 0, 11, "filesystems",data_proc_filesystems },
	{ 11,            REG,    1, 0, 10, "interrupts", data_proc_interrupts },
	{ PROC_KMSG_INO, REGUSR, 1, 0, 4,  "kmsg",       NULL },
	{ 13,            REG,    1, 0, 7,  "loadavg",    data_proc_loadavg },
	{ 14,            REG,    1, 0, 5,  "locks",      data_proc_locks },
	{ 15,            REG,    1, 0, 7,  "meminfo",    data_proc_meminfo },
	{ 16,            REG,    1, 0, 6,  "mounts",     data_proc_mounts },
	{ 17,            REG,    1, 0, 10, "partitions", data_proc_partitions },
	{ 18,            REG,    1, 0, 3,  "pci",        data_proc_pci },
	{ 19,            REG,    1, 0, 3,  "rtc",        data_proc_rtc },
	{ 20,            LNK,    1, 0, 4,  "self",       data_proc_self },
	{ 21,            REG,    1, 0, 4,  "stat",       data_proc_stat },
	{ 22,            REG,    1, 0, 6,  "uptime",     data_proc_uptime },
	{ 23,            REG,    1, 0, 7,  "version",    data_proc_fullversion },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },
   {	/* [1] /PID/ */
	{ 1000,  DIR,  2, 1, 1,  ".",   NULL },
	{ 1,     DIR,  2, 0, 2,  "..",  NULL },
	{ PROC_PID_FD,      DIRFD,  2, 2, 2,  "fd",       NULL },
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

   {	/* [2] /PID/fd/ */
	{ 2000,  DIRFD,  2, 2, 1,  ".",   NULL },
	{ 1000,  DIR,    2, 2, 2,  "..",  NULL },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },

   {	/* [3] /net/ */
	{ 3,     DIR,  2, 3, 1,  ".",   NULL },
	{ 1,     DIR,  2, 0, 2,  "..",  NULL },
	{ 3001,  REG,  1, 3, 4, "unix", data_proc_unix },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },
   {	/* [4] /sys/ */
	{ 4,     DIR,  2, 4, 1,  ".",       NULL },
	{ 1,     DIR,  2, 0, 2,  "..",      NULL },
	{ 4001,  DIR,  2, 5, 6,  "kernel",  NULL },
	{ 4002,  DIR,  2, 6, 2,  "vm",      NULL },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },
   {	/* [4001] /sys/kernel/ */
	{ 4001,  DIR,  2, 5, 1,  ".",   NULL },
	{ 4,     DIR,  2, 3, 2,  "..",  NULL },
	{ 5001,  REG,  1, 5, 9,  "buffer-nr",  data_proc_buffernr },
	{ 5002,  REG,  1, 5, 10, "domainname", data_proc_domainname },
	{ 5003,  REG,  1, 5, 8,  "file-max",   data_proc_filemax },
	{ 5004,  REG,  1, 5, 7,  "file-nr",    data_proc_filenr },
	{ 5005,  REG,  1, 5, 8,  "hostname",   data_proc_hostname },
	{ 5006,  REG,  1, 5, 9,  "inode-max",  data_proc_inodemax },
	{ 5007,  REG,  1, 5, 8,  "inode-nr",   data_proc_inodenr },
	{ 5008,  REG,  1, 5, 9,  "osrelease",  data_proc_osrelease },
	{ 5009,  REG,  1, 5, 6,  "ostype",     data_proc_ostype },
	{ 5010,  REG,  1, 5, 7,  "version",    data_proc_version },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },
   {	/* [4002] /sys/vm/ */
	{ 4002,  DIR,  2, 6, 1,  ".",   NULL },
	{ 4,     DIR,  2, 3, 2,  "..",  NULL },
	{ 6001,  REG,  1, 6, 22, "dirty_background_ratio", data_proc_dirty_background_ratio },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   }
};

struct procfs_dir_entry *get_procfs_by_inode(struct inode *i)
{
	__ino_t inode;
	int n, lev;
	struct procfs_dir_entry *d;

	inode = i->inode;
	for(lev = 0; procfs_array[lev][0].inode; lev++) {
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
