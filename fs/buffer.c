/*
 * fiwix/fs/buffer.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

/*
 * buffer.c implements a cache with a free list as a doubly circular linked
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
 *              (buffer)          (buffer)          (buffer)
 *    ...
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/buffer.h>
#include <fiwix/devices.h>
#include <fiwix/fs.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/stat.h>

#define BUFFER_HASH(dev, block)	(((__dev_t)(dev) ^ (__blk_t)(block)) % (NR_BUF_HASH))
#define NR_BUFFERS	buffer_table_size / sizeof(struct buffer)
#define NR_BUF_HASH	buffer_hash_table_size / sizeof(unsigned int)

struct buffer *buffer_table;		/* buffer pool */
struct buffer *buffer_head;		/* buffer pool head */
struct buffer **buffer_hash_table;

static struct resource sync_resource = { NULL, NULL };

static void insert_to_hash(struct buffer *buf)
{
	struct buffer **h;
	int i;

	i = BUFFER_HASH(buf->dev, buf->block);
	h = &buffer_hash_table[i];

	if(!*h) {
		*h = buf;
		(*h)->prev_hash = (*h)->next_hash = NULL;
	} else {
		buf->prev_hash = NULL;
		buf->next_hash = *h;
		(*h)->prev_hash = buf;
		*h = buf;
	}
}

static void remove_from_hash(struct buffer *buf)
{
	struct buffer **h;
	int i;

	i = BUFFER_HASH(buf->dev, buf->block);
	h = &buffer_hash_table[i];

	while(*h) {
		if(*h == buf) {
			if((*h)->next_hash) {
				(*h)->next_hash->prev_hash = (*h)->prev_hash;
			}
			if((*h)->prev_hash) {
				(*h)->prev_hash->next_hash = (*h)->next_hash;
			}
			if(h == &buffer_hash_table[i]) {
				*h = (*h)->next_hash;
			}
			break;
		}
		h = &(*h)->next_hash;
	}
}

static void remove_from_free_list(struct buffer *buf)
{
	buf->prev_free->next_free = buf->next_free;
	buf->next_free->prev_free = buf->prev_free;
	if(buf == buffer_head) {
		buffer_head = buf->next_free;
	}
}

static void buffer_wait(struct buffer *buf)
{
	unsigned long int flags;

	for(;;) {
		SAVE_FLAGS(flags); CLI();
		if(buf->locked) {
			RESTORE_FLAGS(flags);
			sleep(&buffer_wait, PROC_UNINTERRUPTIBLE);
		} else {
			break;
		}
	}
	buf->locked = 1;
	RESTORE_FLAGS(flags);
}

static struct buffer * get_free_buffer(void)
{
	unsigned long int flags;
	struct buffer *buf;

	/* no more buffers on free list */
	if(buffer_head == buffer_head->next_free) {
		return NULL;
	}

	for(;;) {
		SAVE_FLAGS(flags); CLI();
		buf = buffer_head;
		if(buf->locked) {
			RESTORE_FLAGS(flags);
			sleep(&buffer_wait, PROC_UNINTERRUPTIBLE);
		} else {
			break;
		}
	}

	remove_from_free_list(buf);
	buf->locked = 1;

	RESTORE_FLAGS(flags);
	return buf;
}

static void sync_one_buffer(struct buffer *buf)
{
	struct device *d;
	int errno;

	if(!(d = get_device(BLK_DEV, buf->dev))) {
		printk("WARNING: %s(): block device %d,%d not registered!\n", __FUNCTION__, MAJOR(buf->dev), MINOR(buf->dev));
		return;
	}

	if(d->fsop && d->fsop->write_block) {
		errno = d->fsop->write_block(buf->dev, buf->block, buf->data, buf->size);
		if(errno < 0) {
			if(errno == -EROFS) {
				printk("WARNING: %s(): write protection on device %d,%d.\n", __FUNCTION__, MAJOR(buf->dev), MINOR(buf->dev), buf->block);
			} else {
				printk("WARNING: %s(): I/O error on device %d,%d.\n", __FUNCTION__, MAJOR(buf->dev), MINOR(buf->dev), buf->block);
			}
			return;
		}
		buf->dirty = 0;
	} else {
		printk("WARNING: %s(): device %d,%d does not have the write_block() method!\n", __FUNCTION__, MAJOR(buf->dev), MINOR(buf->dev));
	}
}

static struct buffer * search_buffer_hash(__dev_t dev, __blk_t block, int size)
{
	struct buffer *buf;
	int i;

	i = BUFFER_HASH(dev, block);
	buf = buffer_hash_table[i];

	while(buf) {
		if(buf->dev == dev && buf->block == block && buf->size == size) {
			return buf;
		}
		buf = buf->next_hash;
	}

	return NULL;
}

static struct buffer * getblk(__dev_t dev, __blk_t block, int size)
{
	unsigned long int flags;
	struct buffer *buf;

	for(;;) {
		if((buf = search_buffer_hash(dev, block, size))) {
			SAVE_FLAGS(flags); CLI();
			if(buf->locked) {
				RESTORE_FLAGS(flags);
				sleep(&buffer_wait, PROC_UNINTERRUPTIBLE);
				continue;
			}
			buf->locked = 1;
			remove_from_free_list(buf);
			RESTORE_FLAGS(flags);
			return buf;
		}

		if(!(buf = get_free_buffer())) {
			printk("WARNING: %s(): no more buffers on free list!\n", __FUNCTION__);
			sleep(&get_free_buffer, PROC_UNINTERRUPTIBLE);
			continue;
		}

		if(buf->dirty) {
			sync_one_buffer(buf);
		} else {
			if(!buf->data) {
				if(!(buf->data = (char *)kmalloc())) {
					brelse(buf);
					printk("%s(): returning NULL\n", __FUNCTION__);
					return NULL;
				}
				kstat.buffers += (PAGE_SIZE / 1024);
			}
		}

		SAVE_FLAGS(flags); CLI();
		remove_from_hash(buf);	/* remove it from old hash */
		buf->dev = dev;
		buf->block = block;
		buf->size = size;
		insert_to_hash(buf);
		buf->valid = 0;
		RESTORE_FLAGS(flags);
		return buf;
	}
}

struct buffer * get_dirty_buffer(__dev_t dev, __blk_t block, int size)
{
	unsigned long int flags;
	struct buffer *buf;

	for(;;) {
		if((buf = search_buffer_hash(dev, block, size))) {
			if(buf->dirty) {
				SAVE_FLAGS(flags); CLI();
				if(buf->locked) {
					RESTORE_FLAGS(flags);
					sleep(&buffer_wait, PROC_UNINTERRUPTIBLE);
					continue;
				}
				buf->locked = 1;
				remove_from_free_list(buf);
				RESTORE_FLAGS(flags);
				break;
			}
		}
		buf = NULL;
		break;
	}

	return buf;
}

struct buffer * bread(__dev_t dev, __blk_t block, int size)
{
	struct buffer *buf;
	struct device *d;

	if(!(d = get_device(BLK_DEV, dev))) {
		printk("WARNING: %s(): device major %d not found!\n", __FUNCTION__, MAJOR(dev));
		return NULL;
	}

	if((buf = getblk(dev, block, size))) {
		if(!buf->valid) {
			if(d->fsop && d->fsop->read_block) {
				if(d->fsop->read_block(dev, block, buf->data, size) >= 0) {
					buf->valid = 1;
				}
			}
		}
		if(buf->valid) {
			return buf;
		}
		brelse(buf);
	}
	
	printk("WARNING: %s(): returning NULL!\n", __FUNCTION__);
	return NULL;
}

void bwrite(struct buffer *buf)
{
	buf->dirty = 1;
	buf->valid = 1;
	brelse(buf);
}

void brelse(struct buffer *buf)
{
	unsigned long int flags;

	SAVE_FLAGS(flags); CLI();

	if(!buffer_head) {
		buf->prev_free = buf->next_free = buf;
		buffer_head = buf;
	} else {
		buf->next_free = buffer_head;
		buf->prev_free = buffer_head->prev_free;
		buffer_head->prev_free->next_free = buf;
		buffer_head->prev_free = buf;

		/* if not valid place the buffer at the head of the free list */
		if(!buf->valid) {
			buffer_head = buf;
		}
	}
	buf->locked = 0;

	RESTORE_FLAGS(flags);

	wakeup(&get_free_buffer);
	wakeup(&buffer_wait);
}

void sync_buffers(__dev_t dev)
{
	struct buffer *buf;
	int n;

	buf = &buffer_table[0];

	lock_resource(&sync_resource);
	for(n = 0; n < NR_BUFFERS; n++) {
		if(buf->dirty) {
			if(!dev || buf->dev == dev) {
				buffer_wait(buf);
				sync_one_buffer(buf);
				buf->locked = 0;
				wakeup(&buffer_wait);
			}
		}
		buf++;
	}
	unlock_resource(&sync_resource);
}

void invalidate_buffers(__dev_t dev)
{
	unsigned long int flags;
	unsigned int n;
	struct buffer *buf;

	buf = &buffer_table[0];
	SAVE_FLAGS(flags); CLI();

	for(n = 0; n < NR_BUFFERS; n++) {
		if(!buf->locked && buf->dev == dev) {
			buffer_wait(buf);
			remove_from_hash(buf);
			buf->valid = 0;
			buf->locked = 0;
			wakeup(&buffer_wait);
		}
		buf++;
	}

	RESTORE_FLAGS(flags);
	/* FIXME: invalidate_pages(dev); */
}

/*
 * When kernel runs out of pages, kswapd is awaken and it calls this function
 * which goes throught the buffer free list, freeing up to NR_BUF_RECLAIM
 * buffers.
 */
int reclaim_buffers(void)
{
	struct buffer *buf, *first;
	int reclaimed;

	reclaimed = 0;
	first = NULL;

	for(;;) {
		if(!(buf = get_free_buffer())) {
			printk("WARNING: %s(): no more buffers on free list!\n", __FUNCTION__);
			sleep(&get_free_buffer, PROC_UNINTERRUPTIBLE);
			continue;
		}

		remove_from_hash(buf);
		if(buf->dirty) {
			sync_one_buffer(buf);
		}

		/* this ensures the buffer will go to the tail */
		buf->valid = 1;

		if(first) {
			if(first == buf) {
				brelse(buf);
				break;
			}
		} else {
			first = buf;
		}
		if(buf->data) {
			kfree((unsigned int)buf->data);
			buf->data = NULL;
			kstat.buffers -= (PAGE_SIZE / 1024);
			reclaimed++;
			if(reclaimed == NR_BUF_RECLAIM) {
				brelse(buf);
				break;
			}
		}
		brelse(buf);
		do_sched();
	}

	wakeup(&buffer_wait);
	return reclaimed;
}

void buffer_init(void)
{
	struct buffer *buf;
	unsigned int n;

	memset_b(buffer_table, NULL, buffer_table_size);
	memset_b(buffer_hash_table, NULL, buffer_hash_table_size);
	for(n = 0; n < NR_BUFFERS; n++) {
		buf = &buffer_table[n];
		brelse(buf);
	}
}
