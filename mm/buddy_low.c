/*
 * fiwix/mm/buddy_low.c
 *
 * Copyright 2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

/*
 * This buddy algorithm is intended to handle memory requests smaller
 * than a PAGE_SIZE.
 */

#include <fiwix/kernel.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static struct bl_head *freelist[BUDDY_MAX_LEVEL + 1];

static struct bl_head *get_buddy(struct bl_head *block)
{
	int mask;

	mask = 1 << (block->level + 5);
	return (struct bl_head *)((unsigned int)block ^ mask);
}

static void deallocate(struct bl_head *block)
{
	struct bl_head **h, *buddy, *p;
	struct page *pg;
	unsigned int addr, paddr;
	int level;

	level = block->level;
	buddy = get_buddy(block);

	p = freelist[level];
	while(p != NULL) {
		if(p == buddy) {
			break;
		}
		p = p->next;
	}

	if(p == buddy) {
		/* remove buddy from its free list */
		if(buddy->next) {
			buddy->next->prev = buddy->prev;
		}
		if(buddy->prev) {
			buddy->prev->next = buddy->next;
		}
		if(buddy == freelist[level]) {
			freelist[level] = buddy->next;
		}
		/* deallocate block and its buddy as one single block */
		if(level < BUDDY_MAX_LEVEL - 1) {
			if(block > buddy) {
				buddy->level++;
				deallocate(buddy);
			} else {
				block->level++;
				deallocate(block);
			}
		}

		if(level == BUDDY_MAX_LEVEL - 1) {
			addr = (unsigned int)block;
			paddr = V2P(addr);
			pg = &page_table[paddr >> PAGE_SHIFT];
			pg->flags &= ~PAGE_BUDDYLOW;
			kfree(addr);
			kstat.buddy_low_num_pages--;
		}
	} else {
		/* buddy not free, put block on its free list */
		h = &freelist[level];

		if(!*h) {
			*h = block;
			block->prev = block->next = NULL;
		} else {
			block->next = *h;
			block->prev = NULL;
			(*h)->prev = block;
			*h = block;
		}
	}
}

static struct bl_head *allocate(int size)
{
	struct bl_head *block, *buddy;
	struct page *pg;
	unsigned int addr, paddr;
	int level;

	for(level = 0; bl_blocksize[level] < size; level++);

	if(level == BUDDY_MAX_LEVEL) {
		if((addr = kmalloc(PAGE_SIZE))) {
			paddr = V2P(addr);
			pg = &page_table[paddr >> PAGE_SHIFT];
			pg->flags |= PAGE_BUDDYLOW;
			block = (struct bl_head *)addr;
		} else {
			printk("WARNING: %s(): not enough memory!\n", __FUNCTION__);
			return NULL;
		}
		kstat.buddy_low_num_pages++;
		block->prev = block->next = NULL;
		return block;
	}

	if(freelist[level] != NULL) {
		/* we have a block on freelist */
		block = freelist[level];
		if(block->next) {
			block->next->prev = block->prev;
		}
		if(block->prev) {
			block->prev->next = block->next;
		}
		if(block == freelist[level]) {
			freelist[level] = block->next;
		}
	} else {
		/* split a bigger block */
		block = allocate(bl_blocksize[level + 1]);

		if(block != NULL) {
			/* put the buddy on the free list */
			block->level = level;
			buddy = get_buddy(block);
			buddy->level = level;
			buddy->prev = buddy->next = NULL;
			freelist[level] = buddy;
		}
	}

	return block;
}

unsigned int bl_malloc(__size_t size)
{
	struct bl_head *block;
	int level;

	for(level = 0; bl_blocksize[level] < size; level++);

	kstat.buddy_low_count[level]++;
	kstat.buddy_low_mem_requested += bl_blocksize[level];
	block = allocate(size);
	return block ? (unsigned int)(block + 1) : 0;
}

void bl_free(unsigned int addr)
{
	struct bl_head *block;
	int level;

	block = (struct bl_head *)addr;
	block--;
	level = block->level;
	kstat.buddy_low_count[level]--;
	kstat.buddy_low_mem_requested -= bl_blocksize[level];
	deallocate(block);
}

void buddy_low_init(void)
{
	memset_b(freelist, 0, sizeof(freelist));
}
