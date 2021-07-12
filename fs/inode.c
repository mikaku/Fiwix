/*
 * fiwix/fs/inode.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

/*
 * inode.c implements a cache with a free list as a doubly circular linked
 * list and a chained hash table with doubly linked lists.
 *
 * hash table
 * +--------+  +--------------+  +--------------+  +--------------+
 * | index  |  |prev|data|next|  |prev|data|next|  |prev|data|next|
 * |   0   --> | /  |    | --->  <--- |    | --->  <--- |    |  / |
 * +--------+  +--------------+  +--------------+  +--------------+
 * +--------+  +--------------+  +--------------+  +--------------+
 * | index  |  |prev|data|next|  |prev|data|next|  |prev|data|next|
 * |   1   --> | /  |    | --->  <--- |    | --->  <--- |    |  / |
 * +--------+  +--------------+  +--------------+  +--------------+
 *              (inode)           (inode)           (inode)
 *    ...
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define INODE_HASH(dev, inode)	(((__dev_t)(dev) ^ (__ino_t)(inode)) % (NR_INO_HASH))
#define NR_INODES	(inode_table_size / sizeof(struct inode))
#define NR_INO_HASH	(inode_hash_table_size / sizeof(unsigned int))

struct inode *inode_table;		/* inode pool */
struct inode *inode_head;		/* inode pool head */
struct inode **inode_hash_table;

static struct resource sync_resource = { NULL, NULL };

static void insert_to_hash(struct inode *i)
{
	struct inode **h;
	int n;

	n = INODE_HASH(i->dev, i->inode);
	h = &inode_hash_table[n];

	if(!*h) {
		*h = i;
		(*h)->prev_hash = (*h)->next_hash = NULL;
	} else {
		i->prev_hash = NULL;
		i->next_hash = *h;
		(*h)->prev_hash = i;
		*h = i;
	}
}

static void remove_from_hash(struct inode *i)
{
	struct inode **h;
	int n;

	if(!i->inode) {
		return;
	}

	n = INODE_HASH(i->dev, i->inode);
	h = &inode_hash_table[n];

	while(*h) {
		if(*h == i) {
			if((*h)->next_hash) {
				(*h)->next_hash->prev_hash = (*h)->prev_hash;
			}
			if((*h)->prev_hash) {
				(*h)->prev_hash->next_hash = (*h)->next_hash;
			}
			if(h == &inode_hash_table[n]) {
				*h = (*h)->next_hash;
			}
			break;
		}
		h = &(*h)->next_hash;
	}
}

static void remove_from_free_list(struct inode *i)
{
	if(!kstat.free_inodes) {
		return;
	}

	i->prev_free->next_free = i->next_free;
	i->next_free->prev_free = i->prev_free;
	kstat.free_inodes--;
	if(i == inode_head) {
		inode_head = i->next_free;
	}

	if(!kstat.free_inodes) {
		inode_head = NULL;
	}
}

static struct inode * get_free_inode(void)
{
	unsigned long int flags;
	struct inode *i;

	/* no more inodes on free list */
	if(!kstat.free_inodes) {
		return NULL;
	}

	SAVE_FLAGS(flags); CLI();

	i = inode_head;
	remove_from_free_list(i);
	remove_from_hash(i);
	i->i_mode = 0;
	i->i_uid = 0;
	i->i_size = 0;
	i->i_atime = 0;
	i->i_ctime = 0;
	i->i_mtime = 0;
	i->i_gid = 0;
	i->i_nlink = 0;
	i->i_blocks = 0;
	i->i_flags = 0;
	i->locked = 0;
	i->dirty = 0;
	i->mount_point = NULL;
	i->dev = 0;
	i->inode = 0;
	i->count = 0;
	i->rdev = 0;
	i->fsop = NULL;
	i->sb = NULL;
	memset_b(&i->u, NULL, sizeof(i->u));

	RESTORE_FLAGS(flags);
	return i;
}

static int read_inode(struct inode *i)
{
	int errno;

	inode_lock(i);
	if(i->sb && i->sb->fsop && i->sb->fsop->read_inode) {
		errno = i->sb->fsop->read_inode(i);
		inode_unlock(i);
		return errno;
	}
	inode_unlock(i);
	return -EINVAL;
}

static int write_inode(struct inode *i)
{
	int errno;

	errno = 1;

	inode_lock(i);
	if(i->sb && i->sb->fsop && i->sb->fsop->write_inode) {
		errno = i->sb->fsop->write_inode(i);
	} else {
		/* PIPE_DEV inodes can't be flushed on disk */
		i->dirty = 0;
		errno = 0;
	}
	inode_unlock(i);

	return errno;
}

static struct inode * search_inode_hash(__dev_t dev, __ino_t inode)
{
	struct inode *i;
	int n;

	n = INODE_HASH(dev, inode);
	i = inode_hash_table[n];

	while(i) {
		if(i->dev == dev && i->inode == inode) {
			return i;
		}
		i = i->next_hash;
	}

	return NULL;
}

void inode_lock(struct inode *i)
{
	unsigned long int flags;

	for(;;) {
		SAVE_FLAGS(flags); CLI();
		if(i->locked) {
			RESTORE_FLAGS(flags);
			sleep(i, PROC_UNINTERRUPTIBLE);
		} else {
			break;
		}
	}
	i->locked = 1;
	RESTORE_FLAGS(flags);
}

void inode_unlock(struct inode *i)
{
	unsigned long int flags;

	SAVE_FLAGS(flags); CLI();
	i->locked = 0;
	wakeup(i);
	RESTORE_FLAGS(flags);
}

struct inode * ialloc(struct superblock *sb, int mode)
{
	int errno;
	struct inode *i;

	if((i = get_free_inode())) {
		i->sb = sb;
		i->rdev = sb->dev;
		if(i->sb && i->sb->fsop && i->sb->fsop->ialloc) {
			errno = i->sb->fsop->ialloc(i, mode);
		} else {
			printk("WARNING: this filesystem does not have the ialloc() method!\n");
			i->count = 1;
			i->sb = NULL;
			iput(i);
			return NULL;
		}
		if(errno) {
			i->count = 1;
			i->sb = NULL;
			iput(i);
			return NULL;
		}
		i->dev = sb->dev;
		insert_to_hash(i);
		return i;
	}
	printk("WARNING: %s(): no more inodes on free list!\n", __FUNCTION__);
	return NULL;
}

struct inode * iget(struct superblock *sb, __ino_t inode)
{
	unsigned long int flags;
	struct inode *i;

	if(!inode) {
		return NULL;
	}

	for(;;) {
		if((i = search_inode_hash(sb->dev, inode))) {
			inode_lock(i);

			/* update superblock pointer from mount_table */
			//i->sb = sb;	//FIXME: necessary?

			if(i->mount_point) {
				inode_unlock(i);
				i = i->mount_point;
				inode_lock(i);
			}
			/* FIXME: i->locked = 1; ? */
			if(!i->count) {
				remove_from_free_list(i);
			}
			i->count++;
			inode_unlock(i);
			return i;
		}

		if(!(i = get_free_inode())) {
			printk("WARNING: %s(): no more inodes on free list! (%d).\n", __FUNCTION__, kstat.free_inodes);
			return NULL;
		}

		SAVE_FLAGS(flags); CLI();
		i->dev = i->rdev = sb->dev;
		i->inode = inode;
		i->sb = sb;
		i->count = 1;
		RESTORE_FLAGS(flags);
		if(read_inode(i)) {
			iput(i);
			return NULL;
		}
		insert_to_hash(i);
		/* FIXME: i->locked = 1; ? */
		return i;
	}
}

int bmap(struct inode *i, __off_t offset, int mode)
{
	if(i->fsop && i->fsop->bmap) {
		return i->fsop->bmap(i, offset, mode);
	}
	return -EPERM;
}

int check_fs_busy(__dev_t dev, struct inode *root)
{
	struct inode *i;
	unsigned int n;

	i = &inode_table[0];
	for(n = 0; n < NR_INODES; n++, i = &inode_table[n]) {
		if(i->dev == dev && i->count) {
			if(i == root && i->count == 1) {
				continue;
			}
			/* FIXME: to be removed */
			printk("WARNING: root %d with count %d (on dev %d,%d)\n", root->inode, root->count, MAJOR(i->dev), MINOR(i->dev));
			printk("WARNING: inode %d with count %d (on dev %d,%d)\n", i->inode, i->count, MAJOR(i->dev), MINOR(i->dev));
			return 1;
		}
	}
	return 0;
}

void iput(struct inode *i)
{
	unsigned long int flags;

	/* this solves the problem with rmdir('/') and iput(dir) which is NULL */
	if(!i) {
		return;
	}

	if(!i->count) {
		printk("WARNING: %s(): trying to free an already freed inode (%d)!\n", __FUNCTION__, i->inode);
		return;
	}

	if(--i->count > 0) {
		return;
	}

	if(!i->i_nlink) {
		if(i->sb && i->sb->fsop && i->sb->fsop->ifree) {
			inode_lock(i);
			i->sb->fsop->ifree(i);
			inode_unlock(i);
		}
		remove_from_hash(i);
	}
	if(i->dirty) {
		if(write_inode(i)) {
			printk("WARNING: %s(): can't write inode %d (%d,%d), will remain as dirty.\n", __FUNCTION__, i->inode, MAJOR(i->dev), MINOR(i->dev));
			return;
		}
	}

	SAVE_FLAGS(flags); CLI();

	if(!inode_head) {
		i->prev_free = i->next_free = i;
		inode_head = i;
	} else {
		i->next_free = inode_head;
		i->prev_free = inode_head->prev_free;
		inode_head->prev_free->next_free = i;
		inode_head->prev_free = i;
	}
	kstat.free_inodes++;

	RESTORE_FLAGS(flags);
}

void sync_inodes(__dev_t dev)
{
	struct inode *i;
	int n;

	i = &inode_table[0];

	lock_resource(&sync_resource);
	for(n = 0; n < NR_INODES; n++) {
		if(i->dirty) {
			if(!dev || i->dev == dev) {
				if(write_inode(i)) {
					printk("WARNING: %s(): can't write inode %d (%d,%d), will remain as dirty.\n", __FUNCTION__, i->inode, MAJOR(i->dev), MINOR(i->dev));
				}
			}
		}
		i++;
	}
	unlock_resource(&sync_resource);
}

void invalidate_inodes(__dev_t dev)
{
	unsigned long int flags;
	unsigned int n;
	struct inode *i;

	i = &inode_table[0];
	SAVE_FLAGS(flags); CLI();

	for(n = 0; n < NR_INODES; n++) {
		if(i->dev == dev) {
			inode_lock(i);
			remove_from_hash(i);
			inode_unlock(i);
		}
		i++;
	}

	RESTORE_FLAGS(flags);
}

void inode_init(void)
{
	struct inode *i;
	unsigned int n;

	memset_b(inode_table, NULL, inode_table_size);
	memset_b(inode_hash_table, NULL, inode_hash_table_size);
	for(n = 0; n < NR_INODES; n++) {
		i = &inode_table[n];
		i->count = 1;
		iput(i);
	}
}
