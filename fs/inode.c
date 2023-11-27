/*
 * fiwix/fs/inode.c
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
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
#define NR_INO_HASH	(inode_hash_table_size / sizeof(unsigned int))

struct inode *inode_table;		/* inode pool */
struct inode *inode_head;		/* head of free list */
struct inode **inode_hash_table;

static struct resource sync_resource = { 0, 0 };

/* append a new inode into the inode pool */
static struct inode *add_inode(void)
{
	unsigned int flags;
	struct inode *i;

	if(!(i = (struct inode *)kmalloc(sizeof(struct inode)))) {
		return NULL;
	}
	memset_b(i, 0, sizeof(struct inode));

	SAVE_FLAGS(flags); CLI();
	if(!inode_table) {
		inode_table = i;
	} else {
		i->prev = inode_table->prev;
		inode_table->prev->next = i;
	}
	inode_table->prev = i;
	RESTORE_FLAGS(flags);
	kstat.nr_inodes++;
	return i;
}

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

static void insert_on_free_list(struct inode *i)
{
	if(!inode_head) {
		inode_head = i;
	} else {
		i->prev_free = inode_head->prev_free;
		inode_head->prev_free->next_free = i;
	}
	inode_head->prev_free = i;
}

static void remove_from_free_list(struct inode *i)
{
	if(!inode_head) {
		return;
	}

	if(i->next_free) {
		i->next_free->prev_free = i->prev_free;
	}
	if(i->prev_free) {
		if(i != inode_head) {
			i->prev_free->next_free = i->next_free;
		}
	}
	if(!i->next_free) {
		inode_head->prev_free = i->prev_free;
	}
	if(i == inode_head) {
		inode_head = i->next_free;
	}
	i->prev_free = i->next_free = NULL;
}

static struct inode *get_free_inode(void)
{
	unsigned int flags;
	struct inode *i;

	if(kstat.nr_inodes < kstat.max_inodes) {
		if(!(i = add_inode())) {
			return NULL;
		}
		return i;
	}

	SAVE_FLAGS(flags); CLI();
	if(!(i = inode_head)) {
		/* no more inodes on free list */
		RESTORE_FLAGS(flags);
		return NULL;
	}

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
	i->mount_point = NULL;
	i->state = 0;
	i->dev = 0;
	i->inode = 0;
	i->count = 0;
	i->rdev = 0;
	i->fsop = NULL;
	i->sb = NULL;
	memset_b(&i->u, 0, sizeof(i->u));
	RESTORE_FLAGS(flags);
	return i;
}

static int read_inode(struct inode *i)
{
	int errno;

	inode_lock(i);
	errno = i->sb->fsop->read_inode(i);
	inode_unlock(i);
	return errno;
}

static int write_inode(struct inode *i)
{
	int errno;

	if(i->sb && i->sb->fsop && i->sb->fsop->write_inode) {
		errno = i->sb->fsop->write_inode(i);
	} else {
		/* PIPE_DEV inodes can't be flushed on disk */
		i->state &= ~INODE_DIRTY;
		errno = 0;
	}

	return errno;
}

static struct inode *search_inode_hash(__dev_t dev, __ino_t inode)
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

static void wait_on_inode(struct inode *i)
{
	for(;;) {
		if(i->state & INODE_LOCKED) {
			sleep(i, PROC_UNINTERRUPTIBLE);
		} else {
			break;
		}
	}
}

void inode_lock(struct inode *i)
{
	unsigned int flags;

	for(;;) {
		SAVE_FLAGS(flags); CLI();
		if(i->state & INODE_LOCKED) {
			RESTORE_FLAGS(flags);
			sleep(i, PROC_UNINTERRUPTIBLE);
		} else {
			break;
		}
	}
	i->state |= INODE_LOCKED;
	RESTORE_FLAGS(flags);
}

void inode_unlock(struct inode *i)
{
	unsigned int flags;

	SAVE_FLAGS(flags); CLI();
	i->state &= ~INODE_LOCKED;
	wakeup(i);
	RESTORE_FLAGS(flags);
}

struct inode *ialloc(struct superblock *sb, int mode)
{
	int errno;
	struct inode *i;

	if((i = get_free_inode())) {
		i->sb = sb;
		i->rdev = sb->dev;
		if((errno = i->sb->fsop->ialloc(i, mode))) {
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

struct inode *iget(struct superblock *sb, __ino_t inode)
{
	unsigned int flags;
	struct inode *i;

	if(!inode) {
		return NULL;
	}

	for(;;) {
		if((i = search_inode_hash(sb->dev, inode))) {
			SAVE_FLAGS(flags); CLI();
			if(i->state & INODE_LOCKED) {
				sleep(i, PROC_UNINTERRUPTIBLE);
				RESTORE_FLAGS(flags);
				continue;
			}
			inode_lock(i);

			if(i->mount_point) {
				inode_unlock(i);
				i = i->mount_point;
				inode_lock(i);
			}
			if(!i->count) {
				remove_from_free_list(i);
			}
			i->count++;
			inode_unlock(i);
			RESTORE_FLAGS(flags);
			return i;
		}

		if(!(i = get_free_inode())) {
			printk("WARNING: %s(): no more inodes on free list!\n", __FUNCTION__);
			return NULL;
		}

		SAVE_FLAGS(flags); CLI();
		i->dev = i->rdev = sb->dev;
		i->inode = inode;
		i->sb = sb;
		i->count = 1;
		RESTORE_FLAGS(flags);
		if(read_inode(i)) {
			SAVE_FLAGS(flags); CLI();
			i->count = 0;
			insert_on_free_list(i);
			RESTORE_FLAGS(flags);
			return NULL;
		}
		insert_to_hash(i);
		return i;
	}
}

int bmap(struct inode *i, __off_t offset, int mode)
{
	return i->fsop->bmap(i, offset, mode);
}

int check_fs_busy(__dev_t dev, struct inode *root)
{
	struct inode *i;

	i = inode_table;
	while(i) {
		if(i->dev == dev && i->count) {
			if(i == root && i->count == 1) {
				i = i->next;
				continue;
			}
			/* FIXME: to be removed */
			printk("WARNING: root %d with count %d (on dev %d,%d)\n", root->inode, root->count, MAJOR(i->dev), MINOR(i->dev));
			printk("WARNING: inode %d with count %d (on dev %d,%d)\n", i->inode, i->count, MAJOR(i->dev), MINOR(i->dev));
			return 1;
		}
		i = i->next;
	}
	return 0;
}

void iput(struct inode *i)
{
	unsigned int flags;

	/* this solves the problem with rmdir('/') and iput(dir) which is NULL */
	if(!i) {
		return;
	}

	wait_on_inode(i);

	if(!i->count) {
		printk("WARNING: %s(): trying to free an already freed inode (%d)!\n", __FUNCTION__, i->inode);
		return;
	}

	if(--i->count > 0) {
		return;
	}

	inode_lock(i);
	if(!i->i_nlink) {
		if(i->sb && i->sb->fsop && i->sb->fsop->ifree) {
			i->sb->fsop->ifree(i);
		}
		remove_from_hash(i);
	}
	if(i->state & INODE_DIRTY) {
		if(write_inode(i)) {
			printk("WARNING: %s(): can't write inode %d (%d,%d), will remain as dirty.\n", __FUNCTION__, i->inode, MAJOR(i->dev), MINOR(i->dev));
			if(!i->i_nlink) {
				remove_from_hash(i);
			}
			i->count++;
			inode_unlock(i);
			return;
		}
	}
	inode_unlock(i);

	SAVE_FLAGS(flags); CLI();
	insert_on_free_list(i);
	RESTORE_FLAGS(flags);
}

void sync_inodes(__dev_t dev)
{
	struct inode *i;

	i = inode_table;

	lock_resource(&sync_resource);
	while(i) {
		if(i->state & INODE_DIRTY) {
			if(!dev || i->dev == dev) {
				inode_lock(i);
				if(write_inode(i)) {
					printk("WARNING: %s(): can't write inode %d (%d,%d), will remain as dirty.\n", __FUNCTION__, i->inode, MAJOR(i->dev), MINOR(i->dev));
				}
				inode_unlock(i);
			}
		}
		i = i->next;
	}
	unlock_resource(&sync_resource);
}

void invalidate_inodes(__dev_t dev)
{
	unsigned int flags;
	struct inode *i;

	i = inode_table;
	SAVE_FLAGS(flags); CLI();

	while(i) {
		if(i->dev == dev) {
			inode_lock(i);
			remove_from_hash(i);
			inode_unlock(i);
		}
		i = i->next;
	}

	RESTORE_FLAGS(flags);
}

void inode_init(void)
{
	inode_table = inode_head = NULL;
	memset_b(inode_hash_table, 0, inode_hash_table_size);
}
