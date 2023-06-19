/*
 * fiwix/include/fiwix/mm.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_MEMORY_H
#define _FIWIX_MEMORY_H

#include <fiwix/types.h>
#include <fiwix/segments.h>
#include <fiwix/process.h>

/* convert from physical to virtual the addresses below PAGE_OFFSET only */
#define P2V(addr)		(addr < PAGE_OFFSET ? addr + PAGE_OFFSET : addr)

#define V2P(addr)		(addr - PAGE_OFFSET)

#define PAGE_SIZE		4096
#define PAGE_SHIFT		0x0C
#define PAGE_MASK		~(PAGE_SIZE - 1)	/* 0xFFFFF000 */
#define PAGE_ALIGN(addr)	(((addr) + (PAGE_SIZE - 1)) & PAGE_MASK)
#define PT_ENTRIES		(PAGE_SIZE / sizeof(unsigned int))
#define PD_ENTRIES		(PAGE_SIZE / sizeof(unsigned int))

#define PAGE_LOCKED		0x001
#define PAGE_BUDDYLOW		0x010	/* page belongs to buddy_low */
#define PAGE_RESERVED		0x100	/* kernel, BIOS address, ... */
#define PAGE_COW		0x200	/* marked for Copy-On-Write */

#define PFAULT_V		0x01	/* protection violation */
#define PFAULT_W		0x02	/* during write */
#define PFAULT_U		0x04	/* in user mode */

#define GET_PGDIR(address)	((unsigned int)((address) >> 22) & 0x3FF)
#define GET_PGTBL(address)	((unsigned int)((address) >> 12) & 0x3FF)

struct page {
	int page;		/* page number */
	int count;		/* usage counter */
	int flags;
	__ino_t inode;		/* inode of the file */
	__off_t offset;		/* file offset */
	__dev_t dev;		/* device where file resides */
	char *data;		/* page contents */
	struct page *prev_hash;
	struct page *next_hash;
	struct page *prev_free;
	struct page *next_free;
};

extern struct page *page_table;
extern struct page **page_hash_table;

/* values to be determined during system startup */
extern unsigned int page_table_size;		/* size in bytes */
extern unsigned int page_hash_table_size;	/* size in bytes */

extern unsigned int *kpage_dir;


/* buddy_low.c */
static const unsigned int bl_blocksize[] = {
	32,
	64,
	128,
	256,
	512,
	1024,
	2048,
	4096
};

struct bl_head {
	unsigned char level;	/* size class (exponent of the power of 2) */
	struct bl_head *prev;
	struct bl_head *next;
};

unsigned int bl_malloc(__size_t);
void bl_free(unsigned int);
void buddy_low_init(void);

/* alloc.c */
unsigned int kmalloc(__size_t);
void kfree(unsigned int);

/* page.c */
void page_lock(struct page *);
void page_unlock(struct page *);
struct page *get_free_page(void);
struct page *search_page_hash(struct inode *, __off_t);
void release_page(struct page *);
int is_valid_page(int);
void invalidate_inode_pages(struct inode *);
void update_page_cache(struct inode *, __off_t, const char *, int);
int write_page(struct page *, struct inode *, __off_t, unsigned int);
int bread_page(struct page *, struct inode *, __off_t, char, char);
int file_read(struct inode *, struct fd *, char *, __size_t);
void reserve_pages(unsigned int, unsigned int);
void page_init(int);

/* memory.c */
unsigned int map_kaddr(unsigned int, unsigned int, unsigned int, int);
int bss_init(void);
unsigned int setup_tmp_pgdir(unsigned int, unsigned int);
unsigned int get_mapped_addr(struct proc *, unsigned int);
int clone_pages(struct proc *);
int free_page_tables(struct proc *);
unsigned int map_page(struct proc *, unsigned int, unsigned int, unsigned int);
int unmap_page(unsigned int);
void mem_init(void);
void mem_stats(void);

/* swapper.c */
int kswapd(void);

#endif /* _FIWIX_MEMORY_H */
