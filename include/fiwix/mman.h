/*
 * fiwix/include/fiwix/mman.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_MMAN_H
#define _FIWIX_MMAN_H

#include <fiwix/fs.h>

#define PROT_READ	0x1		/* page can be read */
#define PROT_WRITE	0x2		/* page can be written */
#define PROT_EXEC	0x4		/* page can be executed */
#define PROT_NONE	0x0		/* page cannot be accessed */

#define MAP_SHARED	0x01		/* share changes */
#define MAP_PRIVATE	0x02		/* changes are private */
#define MAP_TYPE	0x0f		/* mask for type of mapping */
#define MAP_FIXED	0x10		/* interpret address exactly */
#define MAP_ANONYMOUS	0x20		/* don't use the file descriptor */

#define MAP_GROWSDOWN	0x0100		/* stack-like segment */
#define MAP_DENYWRITE	0x0800		/* -ETXTBSY */
#define MAP_EXECUTABLE	0x1000		/* mark it as a executable */
#define MAP_LOCKED	0x2000		/* pages are locked */

#define ZERO_PAGE	0x80000000	/* this page must be zero-filled */


#define MS_ASYNC	0x1	/* sync memory asynchronously */
#define MS_INVALIDATE	0x2	/* invalidate the caches */
#define MS_SYNC		0x4	/* synchronous memory sync */

#define MCL_CURRENT	1	/* lock all current mappings */
#define MCL_FUTURE	2	/* lock all future mappings */

#define P_TEXT		1	/* text section */
#define P_DATA		2	/* data section */
#define P_BSS		3	/* BSS section */
#define P_HEAP		4	/* heap section (sys_brk()) */
#define P_STACK		5	/* stack section */
#define P_MMAP		6	/* mmap() section */
#define P_SHM		7	/* shared memory section */

/* compatibility flags */
#define MAP_ANON	MAP_ANONYMOUS
#define MAP_FILE	0

struct mmap {
	unsigned int start;
	unsigned int length;
	unsigned int prot;
	unsigned int flags;
	int fd;
	unsigned int offset;
};

void show_vma_regions(struct proc *);
void release_binary(void);
struct vma *find_vma_region(unsigned int);
struct vma *find_vma_intersection(unsigned int, unsigned int);
int expand_heap(unsigned int);
unsigned int get_unmapped_vma_region(unsigned int);
int do_mmap(struct inode *, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, char, char, void *);
int do_munmap(unsigned int, __size_t);
int do_mprotect(struct vma *, unsigned int, __size_t, int);

#endif /* _FIWIX_MMAN_H */
