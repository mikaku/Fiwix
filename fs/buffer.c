/*
 * fiwix/fs/buffer.c
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
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
#include <fiwix/blk_queue.h>

#define BUFFER_HASH(dev, block)	(((__dev_t)(dev) ^ (__blk_t)(block)) % (NR_BUF_HASH))
#define NR_BUF_HASH		(buffer_hash_table_size / sizeof(unsigned int))
#define BUFHEAD_INDEX(size)	((size / BLKSIZE_1K) - 1)

#define NO_GROW		0
#define GROW_IF_NEEDED	1

struct buffer *buffer_table;		/* buffer pool */

/* [0] = 1KB, [1] = 2KB, [2] = unused, [3] = 4KB */
struct buffer *buffer_head[4];		/* heads of free list */
struct buffer *buffer_dirty_head[4];	/* heads of dirty list */
struct buffer *buffer_retained_head[4];	/* heads of retained list */

/*
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
struct buffer **buffer_hash_table;

static struct resource sync_resource = { 0, 0 };

static struct buffer *add_buffer_to_pool(void)
{
	struct buffer *buf;

	if(!(buf = (struct buffer *)kmalloc(sizeof(struct buffer)))) {
		return NULL;
	}
	memset_b(buf, 0, sizeof(struct buffer));

	if(!buffer_table) {
		buffer_table = buf;
	} else {
		buf->prev = buffer_table->prev;
		buffer_table->prev->next = buf;
	}
	buffer_table->prev = buf;
	kstat.nr_buffers++;
	return buf;
}

static void del_buffer_from_pool(struct buffer *buf)
{
	struct buffer *tmp;

	tmp = buf;

	if(!buf->next && !buf->prev) {
		printk("WARNING: %s(): trying to delete an unexistent buffer (block %d).\n", __FUNCTION__, buf->block);
		return;
	}

	if(buf->next) {
		buf->next->prev = buf->prev;
	}
	if(buf->prev) {
		if(buf != buffer_table) {
			buf->prev->next = buf->next;
		}
	}
	if(!buf->next) {
		buffer_table->prev = buf->prev;
	}
	if(buf == buffer_table) {
		buffer_table = buf->next;
	}

	kfree((unsigned int)tmp);
	kstat.nr_buffers--;
}

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

static void insert_on_dirty_list(struct buffer *buf)
{
	struct buffer *h;
	int index;

	index = BUFHEAD_INDEX(buf->size);
	h = buffer_dirty_head[index];

	if(buf->prev_dirty || buf->next_dirty) {
		return;
	}
	if(!h) {
		buffer_dirty_head[index] = buf;
		h = buffer_dirty_head[index];
	} else {
		buf->prev_dirty = h->prev_dirty;
		h->prev_dirty->next_dirty = buf;
	}
	h->prev_dirty = buf;

	kstat.dirty_buffers += (index + 1);
	kstat.nr_dirty_buffers++;
        if(kstat.nr_dirty_buffers > kstat.max_dirty_buffers) {
                wakeup(&kbdflushd);
        }
}

static void remove_from_dirty_list(struct buffer *buf)
{
	struct buffer *h;
	int index;

	index = BUFHEAD_INDEX(buf->size);
	h = buffer_dirty_head[index];

	if(!h) {
		return;
	}

	if(buf->next_dirty) {
		buf->next_dirty->prev_dirty = buf->prev_dirty;
	}
	if(buf->prev_dirty) {
		if(buf != h) {
			buf->prev_dirty->next_dirty = buf->next_dirty;
		}
	}
	if(!buf->next_dirty) {
		h->prev_dirty = buf->prev_dirty;
	}
	if(buf == h) {
		buffer_dirty_head[index] = buf->next_dirty;
	}
	buf->prev_dirty = buf->next_dirty = NULL;

	kstat.dirty_buffers -= (index + 1);
	kstat.nr_dirty_buffers--;
}

static void insert_on_free_list(struct buffer *buf)
{
	struct buffer *h;
	int index;

	index = BUFHEAD_INDEX(buf->size);
	h = buffer_head[index];

	if(!h) {
		buffer_head[index] = buf;
		h = buffer_head[index];
	} else {
		buf->prev_free = h->prev_free;

		/*
		 * If is not marked as valid then this buffer
		 * is placed at the beginning of the free list.
		 */
		if(!(buf->flags & BUFFER_VALID)) {
			buf->next_free = h;
			h->prev_free = buf;
			buffer_head[index] = buf;
			return;
		} else {
			h->prev_free->next_free = buf;
		}
	}
	h->prev_free = buf;
}

static void remove_from_free_list(struct buffer *buf)
{
	struct buffer *h;
	int index;

	index = BUFHEAD_INDEX(buf->size);
	h = buffer_head[index];

	if(!h) {
		return;
	}

	if(buf->next_free) {
		buf->next_free->prev_free = buf->prev_free;
	}
	if(buf->prev_free) {
		if(buf != h) {
			buf->prev_free->next_free = buf->next_free;
		}
	}
	if(!buf->next_free) {
		h->prev_free = buf->prev_free;
	}
	if(buf == h) {
		buffer_head[index] = buf->next_free;
	}
	buf->prev_free = buf->next_free = NULL;
}

static void buffer_wait(struct buffer *buf)
{
	unsigned int flags;

	for(;;) {
		SAVE_FLAGS(flags); CLI();
		if(buf->flags & BUFFER_LOCKED) {
			sleep(&buffer_wait, PROC_UNINTERRUPTIBLE);
		} else {
			break;
		}
		RESTORE_FLAGS(flags);
	}
	buf->flags |= BUFFER_LOCKED;
	RESTORE_FLAGS(flags);
}

static struct buffer *create_buffers(int size)
{
	struct buffer *buf, *prev, *first;
	char *data;
	int n;

	if(!(data = (char *)kmalloc(PAGE_SIZE))) {
		printk("%s(): returning NULL\n", __FUNCTION__);
		return NULL;
	}

	buf = prev = first = NULL;
	for(n = 0; n < (PAGE_SIZE / size); n++) {
		if(!(buf = add_buffer_to_pool())) {
			if(!n) {
				kfree((unsigned int)data);
				return NULL;
			}
			break;
		}
		if(prev) {
			brelse(prev);
			prev->next_sibling = buf;
		}
		buf->data = data + (n * size);
		buf->size = size;
		buf->first_sibling = first;
		kstat.buffers_size += size / 1024;
		prev = buf;
		if(!first) {
			first = buf;
		}
	}
	buf = buf ? buf : prev;
	if(buf) {
		buf->flags |= BUFFER_LOCKED;
	}
	return buf;
}

static struct buffer *get_free_buffer(int mode, int size)
{
	unsigned int flags;
	struct buffer *buf;
	int index;

	index = BUFHEAD_INDEX(size);
	buf = buffer_head[index];

	/*
	 * We check buf->dev to see if this buffer has been already used
	 * and, if that's the case, then we know that there aren't more
	 * unused buffers, so let's try to create new buffers instead of
	 * reusing them.
	 */
	if(!buf || (buf && buf->dev)) {
		if(mode == GROW_IF_NEEDED) {
			if(kstat.free_pages > (kstat.min_free_pages + 1)) {
				if((buf = create_buffers(size))) {
					return buf;
				}
			}
			buf = buffer_head[index];
		}
	}
	if(!buf) {
		/* no more buffers in this free list */
		return NULL;
	}

	for(;;) {
		SAVE_FLAGS(flags); CLI();
		buf = buffer_head[index];
		if(buf->flags & BUFFER_LOCKED) {
			sleep(&buffer_wait, PROC_UNINTERRUPTIBLE);
		} else {
			break;
		}
		RESTORE_FLAGS(flags);
	}

	remove_from_free_list(buf);
	buf->flags |= BUFFER_LOCKED;

	RESTORE_FLAGS(flags);
	return buf;
}

static struct buffer *get_dirty_buffer(int size)
{
	unsigned int flags;
	struct buffer *buf;
	int index;

	index = BUFHEAD_INDEX(size);

	for(;;) {
		SAVE_FLAGS(flags); CLI();
		if(!(buf = buffer_dirty_head[index])) {
			/* no buffers in this dirty list */
			RESTORE_FLAGS(flags);
			return NULL;
		}
		if(buf->flags & BUFFER_LOCKED) {
			sleep(&buffer_wait, PROC_UNINTERRUPTIBLE);
		} else {
			break;
		}
		RESTORE_FLAGS(flags);
	}

	remove_from_dirty_list(buf);
	buf->flags |= BUFFER_LOCKED;

	RESTORE_FLAGS(flags);
	return buf;
}

static int sync_one_buffer(struct buffer *buf)
{
	struct device *d;
	int errno;

	if(!(d = get_device(BLK_DEV, buf->dev))) {
		printk("WARNING: %s(): block device %d,%d not registered!\n", __FUNCTION__, MAJOR(buf->dev), MINOR(buf->dev));
		return 1;
	}

	if((errno = do_blk_request(d, BLK_WRITE, buf)) < 0) {
		if(errno == -EROFS) {
			printk("WARNING: %s(): unable to write block %d, write protection on device %d,%d.\n", __FUNCTION__, buf->block, MAJOR(buf->dev), MINOR(buf->dev));
		} else {
			printk("WARNING: %s(): unable to write block %d, I/O error on device %d,%d.\n", __FUNCTION__, buf->block, MAJOR(buf->dev), MINOR(buf->dev));
		}
		return 1;
	}
	buf->flags &= ~BUFFER_DIRTY;
	return 0;
}

static struct buffer *search_buffer_hash(__dev_t dev, __blk_t block, int size)
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

static struct buffer *getblk(__dev_t dev, __blk_t block, int size)
{
	unsigned int flags;
	struct buffer *buf;

	for(;;) {
		if((buf = search_buffer_hash(dev, block, size))) {
			SAVE_FLAGS(flags); CLI();
			if(buf->flags & BUFFER_LOCKED) {
				sleep(&buffer_wait, PROC_UNINTERRUPTIBLE);
				RESTORE_FLAGS(flags);
				continue;
			}
			buf->flags |= BUFFER_LOCKED;
			remove_from_free_list(buf);
			RESTORE_FLAGS(flags);
			return buf;
		}

		if(!(buf = get_free_buffer(GROW_IF_NEEDED, size))) {
			wakeup(&kswapd);
			sleep(&get_free_buffer, PROC_UNINTERRUPTIBLE);
			continue;
		}

		if(buf->flags & BUFFER_DIRTY) {
			if(!sync_one_buffer(buf)) {
				remove_from_dirty_list(buf);
			}
			brelse(buf);
			continue;
		}

		SAVE_FLAGS(flags); CLI();
		remove_from_hash(buf);	/* remove it from its old hash */
		buf->dev = dev;
		buf->block = block;
		insert_to_hash(buf);
		buf->flags &= ~BUFFER_VALID;
		RESTORE_FLAGS(flags);
		return buf;
	}
}

/* read a group of blocks */
int gbread(struct device *d, struct blk_request *brh)
{
	struct blk_request *br;
	struct buffer *buf;

	br = brh->next_group;
	while(br) {
		if(!(br->flags & BRF_NOBLOCK)) {
			if((buf = getblk(br->dev, br->block, br->size))) {
				br->buffer = buf;
				if(buf->flags & BUFFER_VALID) {
					br = br->next_group;
					continue;
				}
				brh->left++;
				add_blk_request(br);
			} else {
				/* cancel the previous requests already queued */
				/* FIXME: not tested!! */
				br = brh->next_group;
				while(br) {
					if(!br->status) {
						br->status = BR_COMPLETED;
					}
					br = br->next_group;
				}
				return 1;
			}
		}
		br = br->next_group;
	}

	run_blk_request(d);
	if(brh->left) {
		sleep(brh, PROC_UNINTERRUPTIBLE);
	}
	return 0;
}

/* read a single block */
struct buffer *bread(__dev_t dev, __blk_t block, int size)
{
	struct buffer *buf;
	struct device *d;

	if((buf = getblk(dev, block, size))) {
		if(buf->flags & BUFFER_VALID) {
			return buf;
		}
		if(!(d = get_device(BLK_DEV, dev))) {
			printk("WARNING: %s(): device major %d not found!\n", __FUNCTION__, MAJOR(dev));
			return NULL;
		}
		if(do_blk_request(d, BLK_READ, buf) == size) {
			buf->flags |= BUFFER_VALID;
			return buf;
		}
		brelse(buf);
	}
	
	printk("WARNING: %s(): returning NULL!\n", __FUNCTION__);
	return NULL;
}

void bwrite(struct buffer *buf)
{
	buf->flags |= (BUFFER_DIRTY | BUFFER_VALID);
	brelse(buf);
}

void brelse(struct buffer *buf)
{
	unsigned int flags;

	SAVE_FLAGS(flags); CLI();

	if(buf->flags & BUFFER_DIRTY) {
		insert_on_dirty_list(buf);
	}

	insert_on_free_list(buf);
	buf->flags &= ~BUFFER_LOCKED;

	RESTORE_FLAGS(flags);

	wakeup(&get_free_buffer);
	wakeup(&buffer_wait);
}

void sync_buffers(__dev_t dev)
{
	struct buffer *buf, *first;
	int size;

	lock_resource(&sync_resource);
	for(size = BLKSIZE_1K; size <= PAGE_SIZE; size <<= 1) {
		first = NULL;
		for(;;) {
			if(!(buf = get_dirty_buffer(size))) {
				break;
			}
			if(first == buf) {
				insert_on_dirty_list(buf);
				buf->flags &= ~BUFFER_LOCKED;
				wakeup(&buffer_wait);
				break;
			}
			if(!dev || buf->dev == dev) {
				if(sync_one_buffer(buf)) {
					insert_on_dirty_list(buf);
				}
			} else {
				if(!first) {
					first = buf;
				}
				insert_on_dirty_list(buf);
			}
			buf->flags &= ~BUFFER_LOCKED;
			wakeup(&buffer_wait);
		}
	}
	unlock_resource(&sync_resource);
}

void invalidate_buffers(__dev_t dev)
{
	unsigned int flags;
	struct buffer *buf;

	buf = buffer_table;
	SAVE_FLAGS(flags); CLI();

	while(buf) {
		if(!(buf->flags & BUFFER_LOCKED) && buf->dev == dev) {
			buffer_wait(buf);
			remove_from_hash(buf);
			buf->flags &= ~(BUFFER_VALID | BUFFER_LOCKED);
			wakeup(&buffer_wait);
		}
		buf = buf->next;
	}

	RESTORE_FLAGS(flags);
	/* FIXME: invalidate_pages(dev); */
}

static int reclaim_siblings(struct buffer *buf)
{
	struct buffer *orig, *tmp;
	int index;

	index = BUFHEAD_INDEX(buf->size);
	orig = buf;

	if(buf->first_sibling) {
		buf = buf->first_sibling;
	}
	/* check if one of the siblings is locked or dirty */
	do {
		if(buf == orig) {
			buf = buf->next_sibling;
			continue;
		}
		if(buf->flags & (BUFFER_LOCKED | BUFFER_DIRTY)) {
			/*
			 * If one of the siblings is not eligible to be freed up, then
			 * we give up and return without brelse(orig), otherwise
			 * get_free_buffer() will return the same buffer again. So we
			 * must retain 'orig' in a temporary list to be brelse()d later.
			 */
			orig->next_retained = buffer_retained_head[index];
			buffer_retained_head[index] = orig;
			return 1;
		}
		buf = buf->next_sibling;
	} while(buf);

	/* OK, all siblings are eligible to be freed up */
	buf = orig;
	if(buf->first_sibling) {
		buf = buf->first_sibling;
	}
	do {
		if(buf == orig) {
			buf = buf->next_sibling;
			continue;
		}
		tmp = buf;
		buf = buf->next_sibling;
		remove_from_hash(tmp);
		remove_from_free_list(tmp);
		kstat.buffers_size -= tmp->size / 1024;
		del_buffer_from_pool(tmp);
	} while(buf);
	return 0;
}

/*
 * When kernel runs out of pages, kswapd is awaken to call this function which
 * goes across the buffer cache, freeing up to NR_BUF_RECLAIM pages.
 */
int reclaim_buffers(void)
{
	struct buffer *buf, *tmp, *retained;
	int size, found, reclaimed, index;

	found = reclaimed = 0;
	size = BLKSIZE_1K;

	/* iterate through all buffer sizes */
	for(;;) {
		if(size > PAGE_SIZE) {
			if(!found) {
				break;
			}
			size = BLKSIZE_1K;
			found = 0;
		}
		if((buf = get_free_buffer(NO_GROW, size))) {
			found++;
			if(buf->flags & BUFFER_DIRTY) {
				if(!sync_one_buffer(buf)) {
					remove_from_dirty_list(buf);
				}
			}
			if(reclaim_siblings(buf)) {
				continue;
			}
			kfree((unsigned int)(buf->data) & PAGE_MASK);
			remove_from_hash(buf);
			kstat.buffers_size -= buf->size / 1024;
			del_buffer_from_pool(buf);
			reclaimed++;
		}
		if(reclaimed == NR_BUF_RECLAIM) {
			break;
		}
		size <<= 1;
	}

	/* release all retained buffers */
	for(size = BLKSIZE_1K; size <= PAGE_SIZE; size <<= 1) {
		index = BUFHEAD_INDEX(size);
		retained = buffer_retained_head[index];
		while(retained) {
			tmp = retained;
			retained = retained->next_retained;
			brelse(tmp);
		}
		buffer_retained_head[index] = NULL;
	}

	wakeup(&get_free_buffer);
	wakeup(&buffer_wait);

	/*
	 * If some buffers were reclaimed, then wakeup any process
	 * waiting for a new page because release_page() won't do it.
	 */
	if(reclaimed && reclaimed <= NR_BUF_RECLAIM) {
		wakeup(&get_free_page);
	}

	if(!reclaimed) {
		printk("WARNING: %s(): no more buffers on free lists!\n", __FUNCTION__);
	}
	return reclaimed;
}

int kbdflushd(void)
{
	struct buffer *buf, *first;
	int flushed, size;

	for(;;) {
		sleep(&kbdflushd, PROC_UNINTERRUPTIBLE);
		flushed = 0;

		lock_resource(&sync_resource);
		for(size = BLKSIZE_1K; size <= PAGE_SIZE; size <<= 1) {
			first = NULL;
			for(;;) {
				if(!(buf = get_dirty_buffer(size))) {
					break;
				}

				if(!(buf->flags & BUFFER_DIRTY)) {
					printk("WARNING: %s(): a dirty buffer (dev %x, block %d, flags = %x) is not marked as dirty!\n", __FUNCTION__, buf->dev, buf->block, buf->flags);
					continue;
				}

				if(first) {
					if(first == buf) {
						insert_on_dirty_list(buf);
						buf->flags &= ~BUFFER_LOCKED;
						wakeup(&buffer_wait);
						break;
					}
				} else {
					first = buf;
				}

				if(sync_one_buffer(buf)) {
					insert_on_dirty_list(buf);
					buf->flags &= ~BUFFER_LOCKED;
					wakeup(&buffer_wait);
					continue;
				}
				buf->flags &= ~BUFFER_LOCKED;
				wakeup(&buffer_wait);
				flushed++;

				if(flushed == NR_BUF_RECLAIM) {
					if(kstat.nr_dirty_buffers < kstat.max_dirty_buffers) {
						break;
					}
					flushed = 0;
					do_sched();
				}
			}
		}
		unlock_resource(&sync_resource);
	}
}

void buffer_init(void)
{
	buffer_table = NULL;
	memset_b(buffer_head, 0, sizeof(buffer_head));
	memset_b(buffer_dirty_head, 0, sizeof(buffer_dirty_head));
	memset_b(buffer_retained_head, 0, sizeof(buffer_retained_head));
	kstat.max_dirty_buffers = (kstat.max_buffers_size * BUFFER_DIRTY_RATIO) / 100;
	memset_b(buffer_hash_table, 0, buffer_hash_table_size);
}
