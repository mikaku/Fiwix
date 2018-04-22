/*
 * fiwix/fs/locks.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/errno.h>
#include <fiwix/types.h>
#include <fiwix/locks.h>
#include <fiwix/fs.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static struct resource flock_resource = { NULL, NULL };

static struct flock_file * get_new_flock(struct inode *i)
{
	int n;
	struct flock_file *ff;

	lock_resource(&flock_resource);

	for(n = 0; n < NR_FLOCKS; n++) {
		ff = &flock_file_table[n];
		if(!ff->inode) {
			ff->inode = i;	/* mark it as busy */
			unlock_resource(&flock_resource);
			return ff;
		}
	}

	printk("WARNING: %s(): no more free slots in flock file table.\n");
	unlock_resource(&flock_resource);
	return NULL;
}

static void release_flock(struct flock_file *ff)
{
	memset_b(ff, 0, sizeof(struct flock_file));
}

static struct flock_file * get_flock_file(struct inode *i, int op, struct proc *p)
{
	int n;
	struct flock_file *ff;

	lock_resource(&flock_resource);

	ff = NULL;
	for(n = 0; n < NR_FLOCKS; n++) {
		ff = &flock_file_table[n];
		if(ff->inode != i) {
			continue;
		}
		if(p && p != ff->proc) {
			continue;
		}
		break;
	}
	unlock_resource(&flock_resource);
	return ff;
}

int posix_lock(int ufd, int cmd, struct flock *fl)
{
	int n;
	struct flock_file *ff;
	struct inode *i;
	unsigned char type;

	lock_resource(&flock_resource);
	i = fd_table[current->fd[ufd]].inode;
	for(n = 0; n < NR_FLOCKS; n++) {
		ff = &flock_file_table[n];
		if(ff->inode != i) {
			continue;
		}
		break;
	}
	unlock_resource(&flock_resource);
	if(cmd == F_GETLK) {
		if(ff->inode == i) {
			fl->l_type = ff->type & LOCK_SH ? F_RDLCK : F_WRLCK;
			fl->l_whence = SEEK_SET;
			fl->l_start = 0;
			fl->l_len = 0;
			fl->l_pid = ff->proc->pid;
		} else {
			fl->l_type = F_UNLCK;
		}
	}

	switch(fl->l_type) {
		case F_RDLCK:
			type = LOCK_SH;
			break;
		case F_WRLCK:
			type = LOCK_EX;
			break;
		case F_UNLCK:
			type = LOCK_UN;
			break;
		default:
			return -EINVAL;
	}
	if(cmd == F_SETLK) {
		return flock_inode(i, type);
	}
	if(cmd == F_SETLKW) {
		return flock_inode(i, type | LOCK_NB);
	}
	return 0;
}

void flock_release_inode(struct inode *i)
{
	int n;
	struct flock_file *ff;

	lock_resource(&flock_resource);
	for(n = 0; n < NR_FLOCKS; n++) {
		ff = &flock_file_table[n];
		if(ff->inode != i) {
			continue;
		}
		if(ff->proc != current) {
			continue;
		}
		wakeup(ff);
		release_flock(ff);
	}
	unlock_resource(&flock_resource);
}

int flock_inode(struct inode *i, int op)
{
	int n;
	struct flock_file *ff, *new;

	if(op & LOCK_UN) {
		if((ff = get_flock_file(i, op, current))) {
			wakeup(ff);
			release_flock(ff);
		}
		return 0;
	}

loop:
	lock_resource(&flock_resource);
	new = NULL;
	for(n = 0; n < NR_FLOCKS; n++) {
		ff = &flock_file_table[n];
		if(ff->inode != i) {
			continue;
		}
		if(op & LOCK_SH) {
			if(ff->type & LOCK_EX) {
				if(ff->proc == current) {
					new = ff;
					wakeup(ff);
					break;
				}
				unlock_resource(&flock_resource);
				if(op & LOCK_NB) {
					return -EWOULDBLOCK;
				}
				if(sleep(ff, PROC_INTERRUPTIBLE)) {
					return -EINTR;
				}
				goto loop;
			}
		}
		if(op & LOCK_EX) {
			if(ff->proc == current) {
				new = ff;
				continue;
			}
			unlock_resource(&flock_resource);
			if(op & LOCK_NB) {
				return -EWOULDBLOCK;
			}
			if(sleep(ff, PROC_INTERRUPTIBLE)) {
				return -EINTR;
			}
			goto loop;
		}
	}
	unlock_resource(&flock_resource);

	if(!new) {
		if(!(new = get_new_flock(i))) {
			return -ENOLCK;
		}
	}
	new->inode = i;
	new->type = op;
	new->proc = current;

	return 0;
}

void flock_init(void)
{
	memset_b(flock_file_table, NULL, sizeof(flock_file_table));
}
