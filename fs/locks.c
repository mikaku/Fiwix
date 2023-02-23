/*
 * fiwix/fs/locks.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/errno.h>
#include <fiwix/types.h>
#include <fiwix/locks.h>
#include <fiwix/fs.h>
#include <fiwix/mm.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct flock_file *flock_file_table = NULL;

static struct resource flock_resource = { 0, 0 };

static struct flock_file *get_new_flock(struct inode *i)
{
	struct flock_file *ff;

	if(kstat.nr_flocks + 1 > NR_FLOCKS) {
		printk("WARNING: tried to exceed NR_FLOCKS (%d).\n", NR_FLOCKS);
		return NULL;
	}

	if(!(ff = (struct flock_file *)kmalloc2(sizeof(struct flock_file)))) {
		return NULL;
	}
	memset_b(ff, 0, sizeof(struct flock_file));

	lock_resource(&flock_resource);

	ff->inode = i;	/* mark it as busy */

	if(!flock_file_table) {
		flock_file_table = ff;
	} else {
		ff->prev = flock_file_table->prev;
		flock_file_table->prev->next = ff;
	}
	flock_file_table->prev = ff;

	unlock_resource(&flock_resource);
	kstat.nr_flocks++;

	return ff;
}

static void release_flock(struct flock_file *ff)
{
	unsigned long int flags;
	struct flock_file *tmp;

	tmp = ff;

	if(!ff->next && !ff->prev) {
		printk("WARNING: %s(): trying to release an unexistent flock.\n", __FUNCTION__);
		return;
	}

	SAVE_FLAGS(flags); CLI();
	if(ff->next) {
		ff->next->prev = ff->prev;
	}
	if(ff->prev) {
		if(ff != flock_file_table) {
			ff->prev->next = ff->next;
		}
	}
	if(!ff->next) {
		flock_file_table->prev = ff->prev;
	}
	if(ff == flock_file_table) {
		flock_file_table = ff->next;
	}
	RESTORE_FLAGS(flags);

	kfree2((unsigned int)tmp);
	kstat.nr_flocks--;
}

static struct flock_file *get_flock_file(struct inode *i, struct proc *p)
{
	struct flock_file *ff;

	lock_resource(&flock_resource);
	ff = flock_file_table;

	while(ff) {
		if(ff->inode == i && p && p == ff->proc) {
			break;
		}
		ff = ff->next;
	}
	unlock_resource(&flock_resource);
	return ff;
}

int posix_lock(int ufd, int cmd, struct flock *fl)
{
	struct flock_file *ff;
	struct inode *i;
	unsigned char type;

	lock_resource(&flock_resource);
	ff = flock_file_table;
	i = fd_table[current->fd[ufd]].inode;

	while(ff) {
		if(ff->inode == i) {
			break;;
		}
		ff = ff->next;
	}
	unlock_resource(&flock_resource);

	if(cmd == F_GETLK) {
		if(ff && ff->inode == i) {
			fl->l_type = (ff->type & LOCK_SH) ? F_RDLCK : F_WRLCK;
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
	struct flock_file *ff;

	lock_resource(&flock_resource);
	ff = flock_file_table;

	while(ff) {
		if(ff->inode == i && ff->proc == current) {
			wakeup(ff);
			release_flock(ff);
		}
		ff = ff->next;
	}
	unlock_resource(&flock_resource);
}

int flock_inode(struct inode *i, int op)
{
	struct flock_file *ff, *new;

	if(op & LOCK_UN) {
		if((ff = get_flock_file(i, current))) {
			wakeup(ff);
			release_flock(ff);
		}
		return 0;
	}

loop:
	lock_resource(&flock_resource);
	ff = flock_file_table;
	new = NULL;
	while(ff) {
		if(ff->inode == i) {
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
					ff = ff->next;
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
		ff = ff->next;
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
