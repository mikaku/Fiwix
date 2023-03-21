/*
 * fiwix/mm/alloc.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

/*
 * The kmalloc() function acts like a front-end for the two memory
 * memory allocators currently supported:
 *
 * - buddy_low() for requests up to 2048KB.
 * - get_free_page() rest of requests up to PAGE_SIZE.
 */
unsigned int kmalloc(__size_t size)
{
	struct page *pg;
	int max_size;
	unsigned int addr;

	/* check if size can be managed by buddy_low */
	max_size = bl_blocksize[BUDDY_MAX_LEVEL - 1];
	if(size + sizeof(struct bl_head) <= max_size) {
		size += sizeof(struct bl_head);
		return bl_malloc(size);
	}

	/* FIXME: pending to implement buddy_high */
	if(size > PAGE_SIZE) {
		printk("WARNING: %s(): size (%d) is bigger than PAGE_SIZE!\n", __FUNCTION__, size);
		return 0;
	}

	if((pg = get_free_page())) {
		addr = pg->page << PAGE_SHIFT;
		return P2V(addr);
	}

	/* out of memory! */
	return 0;
}

void kfree(unsigned int addr)
{
	struct page *pg;
	unsigned paddr;

	paddr = V2P(addr);
	pg = &page_table[paddr >> PAGE_SHIFT];

	if(pg->flags & PAGE_BUDDYLOW) {
		bl_free(addr);
	} else {
		release_page(pg);
	}
}
