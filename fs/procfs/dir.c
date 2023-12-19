/*
 * fiwix/fs/procfs/dir.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_proc.h>
#include <fiwix/dirent.h>
#include <fiwix/stat.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct fs_operations procfs_dir_fsop = {
	0,
	0,

	procfs_dir_open,
	procfs_dir_close,
	procfs_dir_read,
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* llseek */
	procfs_dir_readdir,
	NULL,
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	procfs_bmap,
	procfs_lookup,
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

static void free_all_fdstr(struct procfs_dir_entry *d)
{
	while(d) {
		if((d->inode & 0xF0000000) == PROC_FD_INO) {
			kfree((unsigned int)d->name);
		}
		d++;
	}
}

static int proc_listdir(char *buffer, int count)
{
	int n;
	struct proc *p;
	struct procfs_dir_entry *pd;
	struct procfs_dir_entry d;
	int size;

	size = 0;
	pd = (struct procfs_dir_entry *)buffer;

	FOR_EACH_PROCESS(p) {
		d.inode = PROC_PID_INO + (p->pid << 12);
		d.mode = S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
		d.nlink = 1;
		d.lev = -1;
		d.name_len = 1;
		n = p->pid;
		while(n / 10) {
			n /= 10;
			d.name_len++;
		}
		d.name = p->pidstr;
		d.data_fn = NULL;

		if(size + sizeof(struct procfs_dir_entry) > (count - 1)) {
			printk("WARNING: kmalloc() is limited to 4096 bytes.\n");
			break;
		}

		size += sizeof(struct procfs_dir_entry);
		memcpy_b((void *)pd, (void *)&d, sizeof(struct procfs_dir_entry));
		pd++;
		p = p->next;
	}
	return size;
}

static int proc_listfd(struct inode *i, char *buffer, int count)
{
	int n;
	struct proc *p;
	struct procfs_dir_entry *pd;
	struct procfs_dir_entry d;
	char *fdstr;
	int size;

	size = 0;
	pd = (struct procfs_dir_entry *)buffer;

	p = get_proc_by_pid((i->inode >> 12) & 0xFFFF);
	for(n = 0; n < OPEN_MAX; n++) {
		if(p->fd[n]) {
			d.inode = PROC_FD_INO + (p->pid << 12) + n;
			d.mode = S_IFLNK | S_IRWXU;
			d.nlink = 1;
			d.lev = -1;
			if(!(fdstr = (char *)kmalloc(4 + 1))) {
				break;
			}
			d.name_len = sprintk(fdstr, "%d", n);
			d.name = fdstr;
			d.data_fn = NULL;

			if(size + sizeof(struct procfs_dir_entry) > (count - 1)) {
				printk("WARNING: kmalloc() is limited to 4096 bytes.\n");
				break;
			}

			size += sizeof(struct procfs_dir_entry);
			memcpy_b((void *)pd, (void *)&d, sizeof(struct procfs_dir_entry));
			pd++;
		}
	}
	memset_b((void *)pd + size, 0, sizeof(struct procfs_dir_entry));
	return size;
}

static int dir_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	__size_t total_read;
	unsigned int bytes;
	int len, lev;
	char *buf;

	if(!(buf = (void *)kmalloc(PAGE_SIZE))) {
		return -ENOMEM;
	}
	memset_b(buf, 0, PAGE_SIZE);

	/* create the list of directories for each process */
	len = 0;
	if(i->inode == PROC_ROOT_INO) {
		len = proc_listdir(buf, count);
	}

	/* create the list of fds used for each process */
	if((i->inode & 0xF0000FFF) == PROC_PID_FD) {
		len = proc_listfd(i, buf, count);
	}

	/* add the rest of static files in the main directory */
	lev = i->u.procfs.i_lev;

	/* calculate the size of the level without the last entry (NULL) */
	bytes = sizeof(procfs_array[lev]) - sizeof(struct procfs_dir_entry);

	if((len + bytes) > (count - 1)) {
		printk("WARNING: %s(): len (%d) > count (%d).\n", __FUNCTION__, len, count);
		free_all_fdstr((struct procfs_dir_entry *)buf);
		kfree((unsigned int)buf);
		return 0;
	}
	memcpy_b(buf + len, (char *)&procfs_array[lev], bytes);
	len += bytes;
	total_read = fd_table->offset = len;
	memcpy_b(buffer, buf, len);
	kfree((unsigned int)buf);
	return total_read;
}

int procfs_dir_open(struct inode *i, struct fd *fd_table)
{
	fd_table->offset = 0;
	return 0;
}

int procfs_dir_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int procfs_dir_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	return -EISDIR;
}

int procfs_dir_readdir(struct inode *i, struct fd *fd_table, struct dirent *dirent, __size_t count)
{
	unsigned int offset, boffset, dirent_offset, doffset;
	int dirent_len;
	__size_t total_read;
	struct procfs_dir_entry *d;
	int base_dirent_len;
	char *buffer;

	if(!(buffer = (void *)kmalloc(PAGE_SIZE))) {
		return -ENOMEM;
	}

	base_dirent_len = sizeof(dirent->d_ino) + sizeof(dirent->d_off) + sizeof(dirent->d_reclen);

	offset = fd_table->offset;
	boffset = dirent_offset = doffset = 0;

	boffset = offset % PAGE_SIZE;

	total_read = dir_read(i, fd_table, buffer, PAGE_SIZE);
	if((count = MIN(total_read, count)) == 0) {
		kfree((unsigned int)buffer);
		return dirent_offset;
	}

	while(boffset < fd_table->offset) {
		d = (struct procfs_dir_entry *)(buffer + boffset);
		if(!d->inode) {
			break;
		}
		dirent_len = (base_dirent_len + (d->name_len + 1)) + 3;
		dirent_len &= ~3;	/* round up */
		if((doffset + dirent_len) <= count) {
			boffset += sizeof(struct procfs_dir_entry);
			offset += sizeof(struct procfs_dir_entry);
			doffset += dirent_len;
			dirent->d_ino = d->inode;
			dirent->d_off = offset;
			dirent->d_reclen = dirent_len;
			memcpy_b(dirent->d_name, d->name, d->name_len);
			dirent->d_name[d->name_len] = 0;
			dirent = (struct dirent *)((char *)dirent + dirent_len);
			dirent_offset += dirent_len;
		} else {
			break;
		}
		if((d->inode & 0xF0000000) == PROC_FD_INO) {
			kfree((unsigned int)d->name);
		}
	}
	fd_table->offset = boffset;
	kfree((unsigned int)buffer);
	return dirent_offset;
}
