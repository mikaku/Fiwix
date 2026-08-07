/*
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_vm.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/process.h>
#include <fiwix/string.h>

#define HOST_RAM_BASE		0x41000000U
#define HOST_RAM_PAGES		64U
#define HOST_PROT_READ		0x1
#define HOST_PROT_WRITE		0x2
#define HOST_MAP_PRIVATE	0x2
#define HOST_MAP_ANONYMOUS	0x20
#define HOST_MAP_FIXED_NOREPLACE 0x100000

extern void *mmap(void *, unsigned long, int, int, int, long);
extern int munmap(void *, unsigned long);

char _etext[1];
char _edata[1];
char _end[1];

static struct page host_pages[ARM_MEMORY_LIMIT / PAGE_SIZE];
static unsigned int host_next_page;
static unsigned int host_tlb_flushes;

struct page *page_table = host_pages;
struct proc *current;
unsigned int shm_rss;

__addr_t kmalloc(__size_t size)
{
	__addr_t address;
	int page;

	if(size != PAGE_SIZE || host_next_page == HOST_RAM_PAGES) {
		return 0;
	}
	address = HOST_RAM_BASE + host_next_page * PAGE_SIZE;
	host_next_page++;
	page = PHYS_TO_PAGE(address);
	host_pages[page].count++;
	return address;
}

void kfree(__addr_t address)
{
	int page;

	page = PHYS_TO_PAGE(address);
	if(is_valid_page(page) && host_pages[page].count) {
		host_pages[page].count--;
	}
}

int is_valid_page(int page)
{
	return page >= 0 &&
		page < (int)(ARM_MEMORY_LIMIT / PAGE_SIZE);
}

void invalidate_tlb(void)
{
	host_tlb_flushes++;
}

void memcpy_b(void *destination, const void *source, unsigned int count)
{
	unsigned char *to;
	const unsigned char *from;

	to = (unsigned char *)destination;
	from = (const unsigned char *)source;
	while(count--) {
		*to++ = *from++;
	}
}

void memset_b(void *destination, unsigned char value, unsigned int count)
{
	unsigned char *to;

	to = (unsigned char *)destination;
	while(count--) {
		*to++ = value;
	}
}

void printk(const char *format, ...)
{
	(void)format;
}

int write_page(struct page *page, struct inode *inode, __off_t offset,
	unsigned int count)
{
	(void)page;
	(void)inode;
	(void)offset;
	(void)count;
	return 0;
}

static int host_ram_init(void)
{
	void *result;
	unsigned int n;

	result = mmap((void *)(unsigned long)HOST_RAM_BASE,
		HOST_RAM_PAGES * PAGE_SIZE,
		HOST_PROT_READ | HOST_PROT_WRITE,
		HOST_MAP_PRIVATE | HOST_MAP_ANONYMOUS |
			HOST_MAP_FIXED_NOREPLACE, -1, 0);
	if(result != (void *)(unsigned long)HOST_RAM_BASE) {
		return -1;
	}
	for(n = 0; n < ARM_MEMORY_LIMIT / PAGE_SIZE; n++) {
		host_pages[n].page = n;
	}
	return 0;
}

int main(void)
{
	struct proc child;
	struct proc parent;
	struct vma writable;
	__addr_t child_page;
	__addr_t parent_page;
	__addr_t parent_physical;
	unsigned int mapped;
	int pages;

	if(host_ram_init()) {
		return 1;
	}
	memset_b(&parent, 0, sizeof(parent));
	memset_b(&child, 0, sizeof(child));
	memset_b(&writable, 0, sizeof(writable));
	arm_process_roots_init();
	if(arm_process_address_space_create(&parent, 0) ||
		arm_process_address_space_create(&child, &parent)) {
		return 2;
	}
	current = &parent;
	parent_page = map_page(&parent, 0x00100000U, 0,
		PROT_READ | PROT_WRITE);
	if(!parent_page || parent.rss != ARM_VM_ROOT_PAGES + 2) {
		return 3;
	}
	*(unsigned int *)(unsigned long)parent_page = 0x13579BDFU;
	mapped = get_mapped_addr(&parent, 0x00100000U);
	parent_physical = mapped & PAGE_MASK;
	if(parent_physical != parent_page ||
		(mapped & (PAGE_PRESENT | PAGE_RW | PAGE_USER)) !=
			(PAGE_PRESENT | PAGE_RW | PAGE_USER) ||
		!host_pages[PHYS_TO_PAGE(parent_physical)].count) {
		return 4;
	}
	if(!map_page(&parent, 0x00100000U, parent_physical,
			PROT_READ | PROT_EXEC) ||
		get_mapped_addr(&parent, 0x00100000U) & PAGE_RW ||
		!map_page(&parent, 0x00100000U, parent_physical,
			PROT_READ | PROT_WRITE) ||
		map_page_flags(&parent, 0x00200000U, 0,
			PROT_READ | PROT_WRITE, PAGE_NOALLOC)) {
		return 5;
	}

	writable.start = 0x00100000U;
	writable.end = writable.start + PAGE_SIZE;
	writable.prot = PROT_READ | PROT_WRITE;
	writable.flags = MAP_PRIVATE;
	parent.vma_table = &writable;
	pages = clone_pages(&child);
	if(pages != 1) {
		return 6;
	}
	child.rss += pages;
	if(get_mapped_addr(&parent, writable.start) & PAGE_RW ||
		get_mapped_addr(&child, writable.start) & PAGE_RW ||
		(get_mapped_addr(&child, writable.start) & PAGE_MASK) !=
			parent_physical ||
		host_pages[PHYS_TO_PAGE(parent_physical)].count != 2 ||
		!(host_pages[PHYS_TO_PAGE(parent_physical)].flags & PAGE_COW)) {
		return 7;
	}

	current = &child;
	if(copy_on_write_page(&writable, writable.start)) {
		return 8;
	}
	child_page = get_mapped_addr(&child, writable.start);
	if(!(child_page & PAGE_RW) ||
		(child_page & PAGE_MASK) == parent_physical ||
		*(unsigned int *)(unsigned long)(child_page & PAGE_MASK) !=
			0x13579BDFU ||
		host_pages[PHYS_TO_PAGE(parent_physical)].count != 1) {
		return 9;
	}
	*(unsigned int *)(unsigned long)(child_page & PAGE_MASK) =
		0x2468ACE0U;
	if(*(unsigned int *)(unsigned long)parent_page != 0x13579BDFU ||
		unmap_page(writable.start) ||
		free_page_tables(&child) != 1) {
		return 10;
	}
	child.rss--;
	if(child.rss != ARM_VM_ROOT_PAGES ||
		arm_process_address_space_release(&child)) {
		return 11;
	}

	current = &parent;
	if(copy_on_write_page(&writable, writable.start) ||
		!(get_mapped_addr(&parent, writable.start) & PAGE_RW) ||
		unmap_page(writable.start) ||
		free_page_tables(&parent) != 1) {
		return 12;
	}
	parent.rss--;
	if(parent.rss != ARM_VM_ROOT_PAGES ||
		arm_process_address_space_release(&parent) ||
		!host_tlb_flushes) {
		return 13;
	}
	if(munmap((void *)(unsigned long)HOST_RAM_BASE,
		HOST_RAM_PAGES * PAGE_SIZE)) {
		return 14;
	}
	return 0;
}
