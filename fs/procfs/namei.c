/*
 * fiwix/fs/procfs/namei.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_proc.h>
#include <fiwix/process.h>
#include <fiwix/stat.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int procfs_lookup(const char *name, struct inode *dir, struct inode **i_res)
{
	int len, lev;
	__ino_t inode;
	__pid_t pid;
	struct proc *p;
	struct procfs_dir_entry *pdirent;

	pid = inode = 0;
	len = strlen(name);
	if((dir->inode & 0xF0000000) == PROC_PID_INO) {
		pid = (dir->inode >> 12) & 0xFFFF;
	}

	lev = bmap(dir, 0, FOR_READING);
	pdirent = procfs_array[lev];
	while(pdirent->inode && !inode) {
		if(len == pdirent->name_len) {
			if(!(strcmp(pdirent->name, name))) {
				inode = pdirent->inode;
				if(pid) {
					inode = (PROC_PID_INO + (pid << 12)) + (inode & 0xFFF);
					if(strcmp(".", name) == 0) {
						inode = dir->inode;
					}
					if(strcmp("..", name) == 0) {
						inode = pdirent->inode;
					}
				}
			}
		}
		if(inode) {
			/*
			 * This prevents a deadlock in iget() when
			 * trying to lock '.' when 'dir' is the same
			 * directory (ls -lai <dir>).
			 */
			if(inode == dir->inode) {
				*i_res = dir;
				return 0;
			}

			if(!(*i_res = iget(dir->sb, inode))) {
				return -EACCES;
			}
			iput(dir);
			return 0;
		}
		pdirent++;
	}

	FOR_EACH_PROCESS(p) {
		if(p->state) {
			if(len == strlen(p->pidstr)) {
				if(!(strcmp(p->pidstr, name))) {
					inode = PROC_PID_INO + (p->pid << 12);
				}
			}
			if(inode) {
				if(!(*i_res = iget(dir->sb, inode))) {
					return -EACCES;
				}
				iput(dir);
				return 0;
			}
		}
	}
	iput(dir);
	return -ENOENT;
}
