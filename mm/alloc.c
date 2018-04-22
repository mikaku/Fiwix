/*
 * fiwix/mm/alloc.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

/*
 * The implementation of kernel memory allocation is extremely simple, it works
 * with a granularity of PAGE_SIZE (4096 bytes). There is indeed a lot of room
 * for improvements here.
 */
unsigned int kmalloc(void)
{
	struct page *pg;
	unsigned int addr;

	if((pg = get_free_page())) {
		addr = pg->page << PAGE_SHIFT;
		return P2V(addr);
	}

	/* out of memory! */
	return 0;
}

void kfree(unsigned int addr)
{
	addr = V2P(addr);
	release_page(addr >> PAGE_SHIFT);
}
