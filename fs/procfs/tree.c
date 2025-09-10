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
   {	/* [lev 0] / */
	{ 1,     DIR,    2, 0, 1,  ".",   NULL },
	{ 2,     DIR,    2, 0, 2,  "..",  NULL },
	{ 3,     DIR,    3, 3, 3,  "bus", NULL },
	{ 4,     DIR,    2, 4, 3,  "net", NULL },
	{ 5,     DIR,    4, 5, 3,  "sys", NULL },
	{ 6,             REG,    1, 0, 9,  "buddyinfo",  data_proc_buddyinfo },
	{ 7,             REG,    1, 0, 7,  "cmdline",    data_proc_cmdline },
	{ 8,             REG,    1, 0, 7,  "cpuinfo",    data_proc_cpuinfo },
	{ 9,             REG,    1, 0, 7,  "devices",    data_proc_devices },
	{ 10,            REG,    1, 0, 3,  "dma",        data_proc_dma },
	{ 11,            REG,    1, 0, 11, "filesystems",data_proc_filesystems },
	{ 12,            REG,    1, 0, 10, "interrupts", data_proc_interrupts },
	{ PROC_KMSG_INO, REGUSR, 1, 0, 4,  "kmsg",       NULL },
	{ 14,            REG,    1, 0, 7,  "loadavg",    data_proc_loadavg },
	{ 15,            REG,    1, 0, 5,  "locks",      data_proc_locks },
	{ 16,            REG,    1, 0, 7,  "meminfo",    data_proc_meminfo },
	{ 17,            REG,    1, 0, 6,  "mounts",     data_proc_mounts },
	{ 18,            REG,    1, 0, 10, "partitions", data_proc_partitions },
	{ 19,            REG,    1, 0, 3,  "pci",        data_proc_pci },
	{ 20,            REG,    1, 0, 3,  "rtc",        data_proc_rtc },
	{ 21,            LNK,    1, 0, 4,  "self",       data_proc_self },
	{ 22,            REG,    1, 0, 4,  "stat",       data_proc_stat },
	{ 23,            REG,    1, 0, 6,  "uptime",     data_proc_uptime },
	{ 24,            REG,    1, 0, 7,  "version",    data_proc_fullversion },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },
   {	/* [lev 1] /PID/ */
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

   {	/* [lev 2] /PID/fd/ */
	{ 2000,  DIRFD,  2, 2, 1,  ".",   NULL },
	{ 1000,  DIR,    2, 2, 2,  "..",  NULL },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },

   {	/* [lev 3] /bus/ */
	{ 3,     DIR,  3, 3, 1,  ".",       NULL },
	{ 1,     DIR,  2, 0, 2,  "..",      NULL },
	{ 3001,  DIR,  2, 6, 3,  "pci",     NULL },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },
   {	/* [lev 4] /net/ */
	{ 4,     DIR,  2, 4, 1,  ".",   NULL },
	{ 1,     DIR,  2, 0, 2,  "..",  NULL },
	{ 4001,  REG,  1, 4, 4, "unix", data_proc_unix },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },
   {	/* [lev 5] /sys/ */
	{ 5,     DIR,  2, 5, 1,  ".",       NULL },
	{ 1,     DIR,  2, 0, 2,  "..",      NULL },
	{ 5001,  DIR,  2, 7, 6,  "kernel",  NULL },
	{ 5002,  DIR,  2, 8, 2,  "vm",      NULL },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },
   {	/* [3001] /bus/pci/ */
	{ 3001,  DIR,  2, 4, 1,  ".",   NULL },
	{ 3,     DIR,  3, 3, 2,  "..",  NULL },
	{ 6001,  REG,  1, 6, 7,  "devices", data_proc_pci_devices },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },
   {	/* [5001] /sys/kernel/ */
	{ 5001,  DIR,  2, 7, 1,  ".",   NULL },
	{ 5,     DIR,  2, 3, 2,  "..",  NULL },
	{ 7001,  REG,  1, 7, 9,  "buffer-nr",  data_proc_buffernr },
	{ 7002,  REG,  1, 7, 10, "domainname", data_proc_domainname },
	{ 7003,  REG,  1, 7, 8,  "file-max",   data_proc_filemax },
	{ 7004,  REG,  1, 7, 7,  "file-nr",    data_proc_filenr },
	{ 7005,  REG,  1, 7, 8,  "hostname",   data_proc_hostname },
	{ 7006,  REG,  1, 7, 9,  "inode-max",  data_proc_inodemax },
	{ 7007,  REG,  1, 7, 8,  "inode-nr",   data_proc_inodenr },
	{ 7008,  REG,  1, 7, 9,  "osrelease",  data_proc_osrelease },
	{ 7009,  REG,  1, 7, 6,  "ostype",     data_proc_ostype },
	{ 7010,  REG,  1, 7, 7,  "version",    data_proc_version },
	{ 0, 0, 0, 0, 0, NULL, NULL }
   },
   {	/* [5002] /sys/vm/ */
	{ 5002,  DIR,  2, 8, 1,  ".",   NULL },
	{ 5,     DIR,  2, 3, 2,  "..",  NULL },
	{ 8001,  REG,  1, 8, 22, "dirty_background_ratio", data_proc_dirty_background_ratio },
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
