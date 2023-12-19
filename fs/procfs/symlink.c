/*
 * fiwix/fs/procfs/symlink.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/buffer.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_proc.h>
#include <fiwix/mm.h>
#include <fiwix/stat.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct fs_operations procfs_symlink_fsop = {
	0,
	0,

	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* read */
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* llseek */
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	procfs_readlink,
	procfs_followlink,
	NULL,			/* bmap */
	NULL,			/* lookup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

int procfs_readlink(struct inode *i, char *buffer, __size_t count)
{
	int size_read;
	struct procfs_dir_entry *d;
	char *buf;

	if((i->inode & 0xF0000000) == PROC_FD_INO) {
		if(!(buf = (char *)kmalloc(PAGE_SIZE))) {
			return -ENOMEM;
		}
		if((size_read = data_proc_pid_fd(buf, (i->inode >> 12) & 0xFFFF, i->inode))) {
			if(size_read > count) {
				size_read = count;
			}
			memcpy_b(buffer, buf, size_read);
		}
		kfree((unsigned int)buf);
		return size_read;
	}

	if(!(d = get_procfs_by_inode(i))) {
		return -EINVAL;
	}

	if(!d->data_fn) {
		return -EINVAL;
	}

	if(!(buf = (char *)kmalloc(PAGE_SIZE))) {
		return -ENOMEM;
	}

	if((size_read = d->data_fn(buf, (i->inode >> 12) & 0xFFFF)) > 0) {
		if(size_read > count) {
			size_read = count;
		}
		memcpy_b(buffer, buf, size_read);
	}
	kfree((unsigned int)buf);
	return size_read;
}

int procfs_followlink(struct inode *dir, struct inode *i, struct inode **i_res)
{
	__ino_t errno;
	__pid_t pid;
	struct proc *p;
	int ufd;

	if(!i) {
		return -ENOENT;
	}
	if(!(S_ISLNK(i->i_mode))) {
		printk("%s(): Oops, inode '%d' is not a symlink (!?).\n", __FUNCTION__, i->inode);
		return 0;
	}

	p = NULL;
	if((pid = (i->inode >> 12) & 0xFFFF)) {
		if(!(p = get_proc_by_pid(pid))) {
			return -ENOENT;
		}
	}

	if((i->inode & 0xF0000000) == PROC_FD_INO) {
		ufd = i->inode & 0xFFF;
		*i_res = fd_table[p->fd[ufd]].inode;
		fd_table[p->fd[ufd]].inode->count++;
		return 0;
	}

	switch(i->inode & 0xF0000FFF) {
		case PROC_PID_CWD:
			if(!p->pwd) {
				return -ENOENT;
			}
			*i_res = p->pwd;
			p->pwd->count++;
			iput(i);
			break;
		case PROC_PID_EXE:
			/*
			 * This assumes that the first entry in the vma_table
			 * contains the program's inode.
			 */
			if(!p->vma_table || !p->vma_table->inode) {
				return -ENOENT;
			}
			*i_res = p->vma_table->inode;
			p->vma_table->inode->count++;
			iput(i);
			break;
		case PROC_PID_ROOT:
			if(!p->root) {
				return -ENOENT;
			}
			*i_res = p->root;
			p->root->count++;
			iput(i);
			break;
		default:
			iput(i);
			if((errno = parse_namei(current->pidstr, dir, i_res, NULL, FOLLOW_LINKS))) {
				return errno;
			}
	}
	return 0;
}
