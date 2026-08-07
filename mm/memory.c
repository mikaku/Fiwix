/*
 * fiwix/mm/memory.c
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
 * Portions Copyright 2024, Greg Haerr.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/asm.h>
#include <fiwix/multiboot1.h>
#include <fiwix/kparms.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/bios.h>
#include <fiwix/ramdisk.h>
#include <fiwix/process.h>
#include <fiwix/buffer.h>
#include <fiwix/fs.h>
#include <fiwix/kexec.h>
#include <fiwix/shm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#ifdef CONFIG_ARCH_RISCV64
#include <fiwix/riscv64_devices.h>
#endif

#ifdef CONFIG_ARCH_RISCV64
#define KERNEL_TEXT_SIZE	((__addr_t)_etext - KERNEL_ADDR)
#define KERNEL_DATA_SIZE	((__addr_t)_edata - (__addr_t)_etext)
#define KERNEL_BSS_SIZE		((__addr_t)_end - (__addr_t)_edata)
#else
#define KERNEL_TEXT_SIZE	((int)_etext - (PAGE_OFFSET + KERNEL_ADDR))
#define KERNEL_DATA_SIZE	((int)_edata - (int)_etext)
#define KERNEL_BSS_SIZE		((int)_end - (int)_edata)
#endif

__pte_t *kpage_dir;

unsigned int proc_table_size = 0;
unsigned int buffer_hash_table_size = 0;
unsigned int inode_table_size = 0;
unsigned int inode_hash_table_size = 0;
unsigned int fd_table_size = 0;
unsigned int page_table_size = 0;
unsigned int page_hash_table_size = 0;

#ifdef CONFIG_ARCH_RISCV64

#define RV_PTE_V	0x001UL
#define RV_PTE_R	0x002UL
#define RV_PTE_W	0x004UL
#define RV_PTE_X	0x008UL
#define RV_PTE_U	0x010UL
#define RV_PTE_A	0x040UL
#define RV_PTE_D	0x080UL
#define RV_PTE_FLAGS	0x3ffUL
#define RV_SATP_PPN	0x00000fffffffffffUL

static int riscv64_pte_is_leaf(__pte_t);
static __addr_t riscv64_pte_physical(__pte_t);
static __pte_t riscv64_table_pte(__addr_t);

static __pte_t *riscv64_root_table(struct proc *p)
{
	__addr_t physical;

	physical = (p->arch.satp & RV_SATP_PPN) << PAGE_SHIFT;
	return (__pte_t *)P2V(physical);
}

int riscv64_address_space_create(struct proc *p)
{
	__pte_t *root;
	__pte_t *low;
	__pte_t *kernel_low;

	root = (__pte_t *)kmalloc(PAGE_SIZE);
	if(!root) {
		return -1;
	}
	low = (__pte_t *)kmalloc(PAGE_SIZE);
	if(!low) {
		kfree((__addr_t)root);
		return -1;
	}
	memcpy_b(root, kpage_dir, PAGE_SIZE);
	if(root[0] & RV_PTE_V) {
		kernel_low = (__pte_t *)P2V(riscv64_pte_physical(root[0]));
		memcpy_b(low, kernel_low, PAGE_SIZE);
		low[RISCV64_FINISHER_PHYSICAL_BASE >> 21] = 0;
		low[RISCV64_PLIC_PHYSICAL_BASE >> 21] = 0;
		low[RISCV64_UART_PHYSICAL_BASE >> 21] = 0;
	} else {
		memset_b(low, 0, PAGE_SIZE);
	}
	root[0] = riscv64_table_pte(V2P((__addr_t)low));
	p->arch.satp = riscv64_make_satp(V2P((__addr_t)root) >> PAGE_SHIFT);
	p->rss += 2;
	return 0;
}

void riscv64_address_space_release(struct proc *p)
{
	__pte_t *root;
	__pte_t *low;
	int count;

	root = riscv64_root_table(p);
	if(!root || root == kpage_dir) {
		return;
	}
	count = free_page_tables(p);
	p->rss -= count;
	if(root[0] & RV_PTE_V && !riscv64_pte_is_leaf(root[0])) {
		low = (__pte_t *)P2V(riscv64_pte_physical(root[0]));
		kfree((__addr_t)low);
		p->rss--;
	}
	kfree((__addr_t)root);
	p->rss--;
	p->arch.satp = 0;
}

static int riscv64_pte_is_leaf(__pte_t pte)
{
	return pte & (RV_PTE_R | RV_PTE_W | RV_PTE_X);
}

static __addr_t riscv64_pte_physical(__pte_t pte)
{
	return (pte >> 10) << PAGE_SHIFT;
}

static __pte_t riscv64_table_pte(__addr_t physical)
{
	return ((physical >> PAGE_SHIFT) << 10) | RV_PTE_V;
}

static __pte_t riscv64_leaf_pte(__addr_t physical, unsigned int prot,
	int flags)
{
	__pte_t pte;

	pte = ((physical >> PAGE_SHIFT) << 10) |
		RV_PTE_V | RV_PTE_U | RV_PTE_A;
	if(prot & (PROT_READ | PROT_WRITE)) {
		pte |= RV_PTE_R;
	}
	if(prot & PROT_WRITE) {
		pte |= RV_PTE_W | RV_PTE_D;
	}
	if(prot & PROT_EXEC) {
		pte |= RV_PTE_X;
	}
	if(flags & PAGE_NOALLOC) {
		pte |= PAGE_NOALLOC;
	}
	return pte;
}

static __pte_t *riscv64_walk(struct proc *p, __addr_t address, int create)
{
	__pte_t *table;
	__pte_t *entry;
	__addr_t next;
	unsigned int index;
	int level;

	table = riscv64_root_table(p);
	if(!table) {
		return NULL;
	}
	for(level = 2; level > 0; level--) {
		index = (address >> (PAGE_SHIFT + level * 9)) & 0x1ff;
		entry = &table[index];
		if(!(*entry & RV_PTE_V)) {
			if(!create || !(next = kmalloc(PAGE_SIZE))) {
				return NULL;
			}
			memset_b((void *)next, 0, PAGE_SIZE);
			*entry = riscv64_table_pte(V2P(next));
			p->rss++;
		} else if(riscv64_pte_is_leaf(*entry)) {
			return NULL;
		}
		table = (__pte_t *)P2V(riscv64_pte_physical(*entry));
	}
	return &table[(address >> PAGE_SHIFT) & 0x1ff];
}

unsigned int map_kaddr(unsigned int *page_dir, unsigned int from,
	unsigned int to, unsigned int addr, int flags)
{
	(void)page_dir;
	(void)from;
	(void)to;
	(void)flags;
	return addr;
}

void bss_init(void)
{
	memset_b(_edata, 0, (__addr_t)_end - (__addr_t)_edata);
}

unsigned int setup_tmp_pgdir(unsigned int magic, unsigned int info)
{
	(void)magic;
	(void)info;
	return 0;
}

__addr_t get_mapped_addr(struct proc *p, __addr_t addr)
{
	__pte_t *entry;
	__addr_t descriptor;

	entry = riscv64_walk(p, addr, 0);
	if(!entry || !(*entry & RV_PTE_V) || !riscv64_pte_is_leaf(*entry)) {
		return 0;
	}
	descriptor = riscv64_pte_physical(*entry) | PAGE_PRESENT;
	if(*entry & RV_PTE_W) {
		descriptor |= PAGE_RW;
	}
	if(*entry & RV_PTE_U) {
		descriptor |= PAGE_USER;
	}
	if(*entry & PAGE_NOALLOC) {
		descriptor |= PAGE_NOALLOC;
	}
	return descriptor;
}

int copy_on_write_page(struct vma *vma, __addr_t addr)
{
	__pte_t *entry;
	__addr_t physical, newaddr;
	struct page *pg;
	int page;

	(void)vma;
	entry = riscv64_walk(current, addr, 0);
	if(!entry || !(*entry & RV_PTE_V)) {
		return 1;
	}
	physical = riscv64_pte_physical(*entry);
	page = PHYS_TO_PAGE(physical);
	if(!is_valid_page(page)) {
		return 1;
	}
	pg = &page_table[page];
	if(!(pg->flags & PAGE_COW)) {
		printk("Oops!, page %d NOT marked for CoW.\n", pg->page);
		return -1;
	}
	if(pg->count > 1) {
		if(!(newaddr = kmalloc(PAGE_SIZE))) {
			return 1;
		}
		memcpy_b((void *)newaddr, (void *)P2V(physical), PAGE_SIZE);
		*entry = riscv64_table_pte(V2P(newaddr)) |
			((*entry & RV_PTE_FLAGS) | RV_PTE_R | RV_PTE_W | RV_PTE_D);
		kfree(P2V(physical));
		invalidate_tlb();
		return 0;
	}
	if(pg->count == 1) {
		*entry |= RV_PTE_R | RV_PTE_W | RV_PTE_D;
		invalidate_tlb();
		return 0;
	}
	return 1;
}

int clone_pages(struct proc *child)
{
	__pte_t *source;
	__pte_t *destination;
	__addr_t address, physical;
	struct page *pg;
	struct vma *vma;
	int pages;

	pages = 0;
	vma = current->vma_table;
	while(vma) {
		if(vma->flags & MAP_SHARED) {
			vma = vma->next;
			continue;
		}
		for(address = vma->start; address < vma->end;
			address += PAGE_SIZE) {
			source = riscv64_walk(current, address, 0);
			if(!source || !(*source & RV_PTE_V)) {
				continue;
			}
			destination = riscv64_walk(child, address, 1);
			if(!destination) {
				return 0;
			}
			if(*source & PAGE_NOALLOC) {
				*destination = *source;
				continue;
			}
			physical = riscv64_pte_physical(*source);
			if(!is_valid_page(PHYS_TO_PAGE(physical))) {
				return 0;
			}
			pg = &page_table[PHYS_TO_PAGE(physical)];
			if(pg->flags & PAGE_RESERVED) {
				continue;
			}
			*source &= ~RV_PTE_W;
			if(vma->prot & PROT_WRITE) {
				pg->flags |= PAGE_COW;
			}
			*destination = *source;
			pg->count++;
			pages++;
		}
		vma = vma->next;
	}
	invalidate_tlb();
	return pages;
}

static int riscv64_table_empty(__pte_t *table)
{
	int n;

	for(n = 0; n < PT_ENTRIES; n++) {
		if(table[n] & RV_PTE_V) {
			return 0;
		}
	}
	return 1;
}

int free_page_tables(struct proc *p)
{
	__pte_t *root;
	__pte_t *middle;
	__pte_t *leaf;
	int first, second, count;

	root = riscv64_root_table(p);
	count = 0;
	for(first = 0; first < PT_ENTRIES; first++) {
		if(!(root[first] & RV_PTE_V) || riscv64_pte_is_leaf(root[first])) {
			continue;
		}
		middle = (__pte_t *)P2V(riscv64_pte_physical(root[first]));
		for(second = 0; second < PT_ENTRIES; second++) {
			if(!(middle[second] & RV_PTE_V) ||
				riscv64_pte_is_leaf(middle[second])) {
				continue;
			}
			leaf = (__pte_t *)P2V(riscv64_pte_physical(middle[second]));
			if(riscv64_table_empty(leaf)) {
				kfree((__addr_t)leaf);
				middle[second] = 0;
				count++;
			}
		}
		if(riscv64_table_empty(middle)) {
			kfree((__addr_t)middle);
			root[first] = 0;
			count++;
		}
	}
	return count;
}

__addr_t map_page(struct proc *p, __addr_t vaddr, __addr_t addr,
	unsigned int prot)
{
	return map_page_flags(p, vaddr, addr, prot, 0);
}

__addr_t map_page_flags(struct proc *p, __addr_t vaddr, __addr_t addr,
	unsigned int prot, int flags)
{
	__pte_t *entry;
	__addr_t physical;

	entry = riscv64_walk(p, vaddr, 1);
	if(!entry) {
		return 0;
	}
	if(!(*entry & RV_PTE_V)) {
		physical = addr;
		if(!physical) {
			if(!(physical = kmalloc(PAGE_SIZE))) {
				return 0;
			}
			physical = V2P(physical);
			p->rss++;
		}
		*entry = riscv64_leaf_pte(physical, prot, flags);
	} else {
		physical = riscv64_pte_physical(*entry);
		if(prot & PROT_WRITE) {
			*entry |= RV_PTE_R | RV_PTE_W | RV_PTE_D;
		}
	}
	if(p == current) {
		invalidate_tlb();
	}
	return P2V(physical);
}

int unmap_page(__addr_t vaddr)
{
	__pte_t *entry;
	__addr_t physical;

	entry = riscv64_walk(current, vaddr, 0);
	if(!entry || !(*entry & RV_PTE_V)) {
		return 1;
	}
	physical = riscv64_pte_physical(*entry);
	if(!(*entry & PAGE_NOALLOC)) {
		kfree(P2V(physical));
	}
	*entry = 0;
	current->rss--;
	invalidate_tlb();
	return 0;
}

void free_vma_pages(struct vma *vma, __addr_t start, __size_t length)
{
	__pte_t *entry;
	__addr_t address, physical;
	struct page *pg;
	unsigned int offset;

	for(address = start; address < start + length; address += PAGE_SIZE) {
		entry = riscv64_walk(current, address, 0);
		if(!entry || !(*entry & RV_PTE_V)) {
			continue;
		}
		physical = riscv64_pte_physical(*entry);
		if(!(*entry & PAGE_NOALLOC) &&
			is_valid_page(PHYS_TO_PAGE(physical))) {
			pg = &page_table[PHYS_TO_PAGE(physical)];
			if(pg->flags & PAGE_RESERVED) {
				continue;
			}
			if(vma->prot & PROT_WRITE && vma->flags & MAP_SHARED) {
				offset = address - vma->start + vma->offset;
				write_page(pg, vma->inode, offset, PAGE_SIZE);
			}
		}
		unmap_page(address);
#ifdef CONFIG_SYSVIPC
		if(vma->object) {
			shm_rss--;
		}
#endif
	}
	current->rss -= free_page_tables(current);
}

#else

unsigned int map_kaddr(unsigned int *page_dir, unsigned int from, unsigned int to, unsigned int addr, int flags)
{
	unsigned int n;
	unsigned int paddr;
	unsigned int *pgtbl;
	unsigned int pde, pte;

	paddr = addr;
	for(n = from; n < to; n += PAGE_SIZE) {
		pde = GET_PGDIR(n);
		pte = GET_PGTBL(n);
		if(!(page_dir[pde] & ~PAGE_MASK)) {
			if (!addr) {
				paddr = kmalloc(PAGE_SIZE);
				if (!paddr) {
					printk("%s(): no memory\n", __FUNCTION__);
					return 0;
				}
				paddr = V2P(paddr);
			}
			page_dir[pde] = paddr | flags;
			memset_b((void *)(paddr + PAGE_OFFSET), 0, PAGE_SIZE);
			paddr += PAGE_SIZE;
		}
		pgtbl = (unsigned int *)((page_dir[pde] & PAGE_MASK) + PAGE_OFFSET);
		pgtbl[pte] = n | flags;
	}

	return paddr;
}

void bss_init(void)
{
	memset_b((void *)((int)_edata), 0, KERNEL_BSS_SIZE);
}

/*
 * This function creates a Page Directory covering all physical memory
 * pages and places it at the end of the memory. This ensures that it
 * won't be clobbered by a large initrd image.
 *
 * It returns the address of the PD to be activated by the CR3 register.
 */
unsigned int setup_tmp_pgdir(unsigned int magic, unsigned int info)
{
	int n, pd;
	unsigned int addr, memksize;
	unsigned int *pgtbl;
	struct multiboot_info *mbi;

	if(magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		/* 4MB of memory assumed */
		memksize = 4096 - 1024;	/* mem_upper */
	} else {
		mbi = (struct multiboot_info *)(PAGE_OFFSET + info);
		if(!(mbi->flags & MULTIBOOT_INFO_MEMORY)) {
			/* 4MB of memory assumed */
			memksize = 4096 - 1024;	/* mem_upper */
		} else {
			memksize = (unsigned int)mbi->mem_upper;
			/* CONFIG_VM_SPLIT22 marks the maximum physical memory supported */
			if(memksize > ((0xFFFFFFFF - PAGE_OFFSET) / 1024)) {
				memksize = (0xFFFFFFFF - PAGE_OFFSET) / 1024;
			}
		}
	}

	addr = PAGE_OFFSET + (memksize * 1024) - memksize;
	addr = PAGE_ALIGN(addr);

	kpage_dir = (unsigned int *)addr;
	memset_b(kpage_dir, 0, PAGE_SIZE);

	addr += PAGE_SIZE;
	pgtbl = (unsigned int *)addr;
	memset_b(pgtbl, 0, memksize);

	for(n = 0; n < (memksize + 1024) / sizeof(unsigned int); n++) {
		pgtbl[n] = (n << PAGE_SHIFT) | PAGE_PRESENT | PAGE_RW;
		if(!(n % 1024)) {
			pd = n / 1024;
			kpage_dir[pd] = (unsigned int)(addr + (PAGE_SIZE * pd) + GDT_BASE) | PAGE_PRESENT | PAGE_RW;
			kpage_dir[GET_PGDIR(PAGE_OFFSET) + pd] = (unsigned int)(addr + (PAGE_SIZE * pd) + GDT_BASE) | PAGE_PRESENT | PAGE_RW;
		}
	}
	return (unsigned int)kpage_dir - PAGE_OFFSET;
}

/* returns the mapped address of a virtual address */
__addr_t get_mapped_addr(struct proc *p, __addr_t addr)
{
	unsigned int *pgdir, *pgtbl;
	unsigned int pde, pte;

	pgdir = (unsigned int *)P2V(p->arch.cr3);
	pde = GET_PGDIR(addr);
	pte = GET_PGTBL(addr);
	pgtbl = (unsigned int *)P2V((pgdir[pde] & PAGE_MASK));
	return pgtbl[pte];
}

int copy_on_write_page(struct vma *vma, __addr_t addr)
{
	unsigned int *pgdir;
	unsigned int *pgtbl;
	unsigned int pde, pte;
	__addr_t newaddr;
	struct page *pg;
	int page;

	pde = GET_PGDIR(addr);
	pte = GET_PGTBL(addr);
	pgdir = (unsigned int *)P2V(current->arch.cr3);
	pgtbl = (unsigned int *)P2V((pgdir[pde] & PAGE_MASK));
	page = PHYS_TO_PAGE(pgtbl[pte] & PAGE_MASK);
	pg = &page_table[page];

	if(pg->count > 1) {
		if(!(pg->flags & PAGE_COW)) {
			printk("Oops!, page %d NOT marked for CoW.\n", pg->page);
			return -1;
		}
		if(!(newaddr = kmalloc(PAGE_SIZE))) {
			printk("%s(): not enough memory!\n", __FUNCTION__);
			return 1;
		}
		current->rss++;
		memcpy_b((void *)newaddr,
			(void *)P2V(PAGE_TO_PHYS(page)), PAGE_SIZE);
		pgtbl[pte] = V2P(newaddr) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
		kfree(P2V(PAGE_TO_PHYS(page)));
		current->rss--;
		invalidate_tlb();
		return 0;
	}
	if(pg->count == 1) {
		if(!(pg->flags & PAGE_COW)) {
			printk("Oops!, last page %d NOT marked for CoW.\n", pg->page);
			return -1;
		}
		pgtbl[pte] = PAGE_TO_PHYS(page) |
			PAGE_PRESENT | PAGE_RW | PAGE_USER;
		invalidate_tlb();
		return 0;
	}
	printk("WARNING: %s(): page %d with pg->count = 0!\n",
		__FUNCTION__, pg->page);
	return 1;
}

int clone_pages(struct proc *child)
{
	unsigned int *src_pgdir, *dst_pgdir;
	unsigned int *src_pgtbl, *dst_pgtbl;
	unsigned int pde, pte;
	unsigned int p_addr;
	__addr_t c_addr;
	unsigned int n, pages;
	struct page *pg;
	struct vma *vma;

	src_pgdir = (unsigned int *)P2V(current->arch.cr3);
	dst_pgdir = (unsigned int *)P2V(child->arch.cr3);
	vma = current->vma_table;
	pages = 0;

	while(vma) {
		if(vma->flags & MAP_SHARED) {
			vma = vma->next;
			continue;
		}
		for(n = vma->start; n < vma->end; n += PAGE_SIZE) {
			pde = GET_PGDIR(n);
			pte = GET_PGTBL(n);
			if(src_pgdir[pde] & PAGE_PRESENT) {
				src_pgtbl = (unsigned int *)P2V((src_pgdir[pde] & PAGE_MASK));
				if(!(dst_pgdir[pde] & PAGE_PRESENT)) {
					if(!(c_addr = kmalloc(PAGE_SIZE))) {
						printk("%s(): returning 0!\n", __FUNCTION__);
						return 0;
					}
					current->rss++;
					pages++;
					dst_pgdir[pde] = V2P(c_addr) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
					memset_b((void *)c_addr, 0, PAGE_SIZE);
				}
				dst_pgtbl = (unsigned int *)P2V((dst_pgdir[pde] & PAGE_MASK));
				if(src_pgtbl[pte] & PAGE_PRESENT) {
					if (src_pgtbl[pte] & PAGE_NOALLOC) {
						dst_pgtbl[pte] = src_pgtbl[pte];
						continue;
					}
					p_addr = PHYS_TO_PAGE(src_pgtbl[pte] & PAGE_MASK);
					pg = &page_table[p_addr];
					if(pg->flags & PAGE_RESERVED) {
						continue;
					}
					src_pgtbl[pte] &= ~PAGE_RW;
					/* mark writable pages as copy-on-write */
					if(vma->prot & PROT_WRITE) {
						pg->flags |= PAGE_COW;
					}
					dst_pgtbl[pte] = src_pgtbl[pte];
					if(!is_valid_page(PHYS_TO_PAGE(dst_pgtbl[pte] & PAGE_MASK))) {
						PANIC("%s: missing page %d during copy-on-write process.\n", __FUNCTION__, PHYS_TO_PAGE(dst_pgtbl[pte] & PAGE_MASK));
					}
					pg = &page_table[PHYS_TO_PAGE(dst_pgtbl[pte] & PAGE_MASK)];
					pg->count++;
				}
			}
		}
		vma = vma->next;
	}
	return pages;
}

int free_page_tables(struct proc *p)
{
	unsigned int *pgdir;
	int n, count;

	pgdir = (unsigned int *)P2V(p->arch.cr3);
	for(n = 0, count = 0; n < PD_ENTRIES; n++) {
		if((pgdir[n] & (PAGE_PRESENT | PAGE_RW | PAGE_USER)) == (PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
			kfree(P2V(pgdir[n]) & PAGE_MASK);
			pgdir[n] = 0;
			count++;
		}
	}
	return count;
}

__addr_t map_page(struct proc *p, __addr_t vaddr, __addr_t addr, unsigned int prot)
{
	return map_page_flags(p, vaddr, addr, prot, 0);
}

__addr_t map_page_flags(struct proc *p, __addr_t vaddr, __addr_t addr, unsigned int prot, int flags)
{
	unsigned int *pgdir, *pgtbl;
	__addr_t newaddr;
	int pde, pte;

	pgdir = (unsigned int *)P2V(p->arch.cr3);
	pde = GET_PGDIR(vaddr);
	pte = GET_PGTBL(vaddr);

	if(!(pgdir[pde] & PAGE_PRESENT)) {	/* allocating page table */
		if(!(newaddr = kmalloc(PAGE_SIZE))) {
			return 0;
		}
		p->rss++;
		pgdir[pde] = V2P(newaddr) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
		memset_b((void *)newaddr, 0, PAGE_SIZE);
	}
	pgtbl = (unsigned int *)P2V((pgdir[pde] & PAGE_MASK));
	if(!(pgtbl[pte] & PAGE_PRESENT)) {	/* allocating page */
		if(!addr) {
			if(!(addr = kmalloc(PAGE_SIZE))) {
				return 0;
			}
			addr = V2P(addr);
			p->rss++;
		}
		pgtbl[pte] = addr | PAGE_PRESENT | PAGE_USER | flags;
	}
	if(prot & PROT_WRITE) {
		pgtbl[pte] |= PAGE_RW;
	}
	return P2V(addr);
}

int unmap_page(__addr_t vaddr)
{
	unsigned int *pgdir, *pgtbl;
	unsigned int desc;
	__addr_t addr;
	int pde, pte;

	pgdir = (unsigned int *)P2V(current->arch.cr3);
	pde = GET_PGDIR(vaddr);
	pte = GET_PGTBL(vaddr);
	if(!(pgdir[pde] & PAGE_PRESENT)) {
		printk("WARNING: %s(): trying to unmap an unallocated pde '0x%08x'\n", __FUNCTION__, vaddr);
		return 1;
	}

	pgtbl = (unsigned int *)P2V((pgdir[pde] & PAGE_MASK));
	if(!(pgtbl[pte] & PAGE_PRESENT)) {
		printk("WARNING: %s(): trying to unmap an unallocated page '0x%08x'\n", __FUNCTION__, vaddr);
		return 1;
	}

	desc = pgtbl[pte];
	addr = desc & PAGE_MASK;
	pgtbl[pte] = 0;
	if (!(desc & PAGE_NOALLOC)) {
		kfree(P2V(addr));
	}
	current->rss--;
	return 0;
}

void free_vma_pages(struct vma *vma, __addr_t start, __size_t length)
{
	unsigned int n, offset;
	unsigned int *pgdir, *pgtbl;
	unsigned int pde, pte;
	struct page *pg;
	int page;

	pgdir = (unsigned int *)P2V(current->arch.cr3);
	pgtbl = NULL;

	for(n = 0; n < (length / PAGE_SIZE); n++) {
		pde = GET_PGDIR(start + (n * PAGE_SIZE));
		pte = GET_PGTBL(start + (n * PAGE_SIZE));
		if(pgdir[pde] & PAGE_PRESENT) {
			pgtbl = (unsigned int *)P2V((pgdir[pde] & PAGE_MASK));
			if(pgtbl[pte] & PAGE_PRESENT) {
				if (!(pgtbl[pte] & PAGE_NOALLOC)) {
					/* make sure to not free reserved pages */
					page = PHYS_TO_PAGE(pgtbl[pte] & PAGE_MASK);
					pg = &page_table[page];
					if(pg->flags & PAGE_RESERVED) {
						continue;
					}

					if(vma->prot & PROT_WRITE && vma->flags & MAP_SHARED) {
						offset = start - vma->start + vma->offset + n * PAGE_SIZE;
						write_page(pg, vma->inode, offset, PAGE_SIZE);
					}

					kfree(P2V(pgtbl[pte]) & PAGE_MASK);
				}
				current->rss--;
#ifdef CONFIG_SYSVIPC
				if(vma->object) {
					shm_rss--;
				}
#endif /* CONFIG_SYSVIPC */
				pgtbl[pte] = 0;

				/* check if a page table can be freed */
				for(pte = 0; pte < PT_ENTRIES; pte++) {
					if(pgtbl[pte] & PAGE_MASK) {
						break;
					}
				}
				if(pte == PT_ENTRIES) {
					kfree((__addr_t)pgtbl & PAGE_MASK);
					current->rss--;
					pgdir[pde] = 0;
				}
			}
		}
	}
}

#endif /* CONFIG_ARCH_RISCV64 */

static int memory_range_available(__addr_t end, unsigned int physical_memory)
{
#ifdef CONFIG_ARCH_RISCV64
	return end <= PHYSICAL_MEMORY_BASE + physical_memory;
#else
	(void)physical_memory;
	return is_addr_in_bios_map(end);
#endif
}

/*
 * This function initializes and setups the kernel page directory and page
 * tables. It also reserves areas of contiguous memory spaces for internal
 * structures and for the RAMdisk drives.
 */
void mem_init(void)
{
	unsigned int sizek;
	unsigned int physical_memory;
	int n, pages, last_ramdisk;

#ifdef CONFIG_ARCH_RISCV64
	__pte_t *device_table;
	__pte_t *low_table;
	unsigned int root_leaves;

	if(!kstat.physical_pages) {
		kstat.physical_pages = RISCV64_MEMORY_FALLBACK >> PAGE_SHIFT;
	}
	physical_memory = (unsigned int)kstat.physical_pages << PAGE_SHIFT;
	if(_last_data_addr < PHYSICAL_MEMORY_BASE) {
		_last_data_addr = (__addr_t)_end;
	}
	_last_data_addr = PAGE_ALIGN(_last_data_addr);
	kpage_dir = (__pte_t *)_last_data_addr;
	memset_b(kpage_dir, 0, PAGE_SIZE);
	_last_data_addr += PAGE_SIZE;
	low_table = (__pte_t *)_last_data_addr;
	memset_b(low_table, 0, PAGE_SIZE);
	_last_data_addr += PAGE_SIZE;
	device_table = (__pte_t *)_last_data_addr;
	memset_b(device_table, 0, PAGE_SIZE);
	_last_data_addr += PAGE_SIZE;

	root_leaves = (physical_memory + 0x3fffffffUL) >> 30;
	for(n = 0; n < (int)root_leaves; n++) {
		kpage_dir[2 + n] =
			(((PHYSICAL_MEMORY_BASE + ((unsigned long)n << 30)) >>
			PAGE_SHIFT) << 10) |
			RV_PTE_V | RV_PTE_R | RV_PTE_W | RV_PTE_X |
			RV_PTE_A | RV_PTE_D;
	}
	kpage_dir[0] = riscv64_table_pte(V2P((__addr_t)low_table));
	kpage_dir[256] = riscv64_table_pte(V2P((__addr_t)device_table));
	low_table[0] = RV_PTE_V | RV_PTE_R | RV_PTE_W | RV_PTE_A | RV_PTE_D;
	low_table[RISCV64_PLIC_PHYSICAL_BASE >> 21] =
		((RISCV64_PLIC_PHYSICAL_BASE >> PAGE_SHIFT) << 10) |
		RV_PTE_V | RV_PTE_R | RV_PTE_W | RV_PTE_A | RV_PTE_D;
	low_table[RISCV64_UART_PHYSICAL_BASE >> 21] =
		((RISCV64_UART_PHYSICAL_BASE >> PAGE_SHIFT) << 10) |
		RV_PTE_V | RV_PTE_R | RV_PTE_W | RV_PTE_A | RV_PTE_D;
	device_table[0] =
		RV_PTE_V | RV_PTE_R | RV_PTE_W | RV_PTE_A | RV_PTE_D;
	device_table[RISCV64_UART_PHYSICAL_BASE >> 21] =
		((RISCV64_UART_PHYSICAL_BASE >> PAGE_SHIFT) << 10) |
		RV_PTE_V | RV_PTE_R | RV_PTE_W | RV_PTE_A | RV_PTE_D;
	riscv64_vm_install(V2P((__addr_t)kpage_dir) >> PAGE_SHIFT);
#else
	unsigned int physical_page_tables;
	unsigned int *pgtbl;

	physical_page_tables = (kstat.physical_pages / 1024) + ((kstat.physical_pages % 1024) ? 1 : 0);
	physical_memory = (kstat.physical_pages << PAGE_SHIFT);	/* in bytes */

	/* align _last_data_addr to the next page */
	_last_data_addr = PAGE_ALIGN(_last_data_addr);

	/* Page Directory */
	kpage_dir = (unsigned int *)_last_data_addr;
	memset_b(kpage_dir, 0, PAGE_SIZE);
	_last_data_addr += PAGE_SIZE;

	/* Page Tables */
	pgtbl = (unsigned int *)_last_data_addr;
	memset_b(pgtbl, 0, physical_page_tables * PAGE_SIZE);
	_last_data_addr += physical_page_tables * PAGE_SIZE;

	/* Page Directory and Page Tables initialization */
	for(n = 0; n < kstat.physical_pages; n++) {
		pgtbl[n] = (n << PAGE_SHIFT) | PAGE_PRESENT | PAGE_RW;
		if(!(n % 1024)) {
			kpage_dir[GET_PGDIR(PAGE_OFFSET) + (n / 1024)] = (unsigned int)&pgtbl[n] | PAGE_PRESENT | PAGE_RW;
		}
	}
	activate_kpage_dir();

	/* since Page Directory is now activated we can use virtual addresses */
	kpage_dir = (unsigned int *)P2V((unsigned int)kpage_dir);
	_last_data_addr = P2V(_last_data_addr);
#endif

	/* reserve memory space for proc_table[NR_PROCS] */
	proc_table_size = PAGE_ALIGN(sizeof(struct proc) * NR_PROCS);
	if(!memory_range_available(V2P(_last_data_addr) + proc_table_size,
		physical_memory)) {
		PANIC("Not enough memory for proc_table.\n");
	}
	proc_table = (struct proc *)_last_data_addr;
	_last_data_addr += proc_table_size;


	/* reserve memory space for buffer_hash_table */
	kstat.max_buffers_size = kstat.physical_pages * (PAGE_SIZE / 1024);
	kstat.max_buffers_size = (kstat.max_buffers_size * BUFFER_PERCENTAGE) / 100;
	n = (kstat.max_buffers_size * BUFFER_HASH_PERCENTAGE) / 100;
	n = MAX(n, 10);	/* 10 buffer hashes as minimum */
	/* buffer_hash_table is an array of pointers */
	pages = ((n * sizeof(*buffer_hash_table)) / PAGE_SIZE) + 1;
	buffer_hash_table_size = pages << PAGE_SHIFT;
	if(!memory_range_available(V2P(_last_data_addr) + buffer_hash_table_size,
		physical_memory)) {
		PANIC("Not enough memory for buffer_hash_table.\n");
	}
	buffer_hash_table = (struct buffer **)_last_data_addr;
	_last_data_addr += buffer_hash_table_size;


	/* calculate the inode table size */
	sizek = physical_memory / 1024;	/* this helps to avoid overflow */
	inode_table_size = (sizek * INODE_PERCENTAGE) / 100;
	inode_table_size *= 1024;
	pages = inode_table_size >> PAGE_SHIFT;
	inode_table_size = pages << PAGE_SHIFT;

	/* reserve memory space for inode_hash_table */
	kstat.max_inodes = inode_table_size / sizeof(struct inode);
	n = (kstat.max_inodes * INODE_HASH_PERCENTAGE) / 100;
	n = MAX(n, 10);	/* 10 inode hash buckets as minimum */
	/* inode_hash_table is an array of pointers */
	pages = ((n * sizeof(*inode_hash_table)) / PAGE_SIZE) + 1;
	inode_hash_table_size = pages << PAGE_SHIFT;
	if(!memory_range_available(V2P(_last_data_addr) + inode_hash_table_size,
		physical_memory)) {
		PANIC("Not enough memory for inode_hash_table.\n");
	}
	inode_hash_table = (struct inode **)_last_data_addr;
	_last_data_addr += inode_hash_table_size;


	/* reserve memory space for fd_table[NR_OPENS] */
	fd_table_size = PAGE_ALIGN(sizeof(struct fd) * NR_OPENS);
	if(!memory_range_available(V2P(_last_data_addr) + fd_table_size,
		physical_memory)) {
		PANIC("Not enough memory for fd_table.\n");
	}
	fd_table = (struct fd *)_last_data_addr;
	_last_data_addr += fd_table_size;


	/* reserve memory space for RAMdisk drives */
	last_ramdisk = 0;
	if(kparms.ramdisksize > 0 || ramdisk_table[0].addr) {
		/*
		 * If the 'initrd=' parameter was supplied, then the first
		 * RAMdisk drive was already assigned to the initrd image.
		 */
		if(ramdisk_table[0].addr) {
#ifndef CONFIG_ARCH_RISCV64
			ramdisk_table[0].addr += PAGE_OFFSET;
#endif
			last_ramdisk = 1;
		}
		for(; last_ramdisk < ramdisk_minors; last_ramdisk++) {
			if(!memory_range_available(V2P(_last_data_addr) +
				(kparms.ramdisksize * 1024), physical_memory)) {
				kparms.ramdisksize = 0;
				ramdisk_minors -= RAMDISK_DRIVES;
				printk("WARNING: RAMdisk drive disabled (not enough physical memory).\n");
				break;
			}
			ramdisk_table[last_ramdisk].addr = (char *)_last_data_addr;
			ramdisk_table[last_ramdisk].size = kparms.ramdisksize;
			_last_data_addr += kparms.ramdisksize * 1024;
		}
	}

	/*
	 * FIXME: this is ugly!
	 * It should go in console_init() once we have a proper kernel memory/page management.
	 */
	#include <fiwix/console.h>
	for(n = 1; n <= NR_VCONSOLES; n++) {
		vc_screen[n] = (short int *)_last_data_addr;
		_last_data_addr += (video.columns * video.lines * 2);
	}
	/*
	 * FIXME: this is ugly!
	 * It should go in console_init() once we have a proper kernel memory/page management.
	 */
	vcbuf = (short int *)_last_data_addr;
	_last_data_addr += (video.columns * video.lines * SCREENS_LOG * 2 * sizeof(short int));

#ifdef CONFIG_KEXEC
	if(kexec_size > 0) {
		bios_map_reserve(KEXEC_BOOT_ADDR, KEXEC_BOOT_ADDR + (PAGE_SIZE * 2));
		ramdisk_minors++;
		if(last_ramdisk < ramdisk_minors) {
			if(!memory_range_available(V2P(_last_data_addr) +
				(kexec_size * 1024), physical_memory)) {
				kexec_size = 0;
				ramdisk_minors--;
				printk("WARNING: RAMdisk drive for kexec disabled (not enough physical memory).\n");
			} else {
				ramdisk_table[last_ramdisk].addr = (char *)_last_data_addr;
				ramdisk_table[last_ramdisk].size = kexec_size;
				_last_data_addr += kexec_size * 1024;
			}
		}
	}
#endif /* CONFIG_KEXEC */

	/* the last one must be the page_table structure */
	n = (kstat.physical_pages * PAGE_HASH_PER_10K) / 10000;
	n = MAX(n, 1);	/* 1 page for the hash table as minimum */
	n = MIN(n, MAX_PAGES_HASH);
	page_hash_table_size = n * PAGE_SIZE;
	if(!memory_range_available(V2P(_last_data_addr) + page_hash_table_size,
		physical_memory)) {
		PANIC("Not enough memory for page_hash_table.\n");
	}
	page_hash_table = (struct page **)_last_data_addr;
	_last_data_addr += page_hash_table_size;

	page_table_size = PAGE_ALIGN(kstat.physical_pages * sizeof(struct page));
	if(!memory_range_available(V2P(_last_data_addr) + page_table_size,
		physical_memory)) {
		PANIC("Not enough memory for page_table.\n");
	}
	page_table = (struct page *)_last_data_addr;
	_last_data_addr += page_table_size;

	page_init(kstat.physical_pages);
	buddy_low_init();
}

void mem_stats(void)
{
	kstat.kernel_reserved <<= 2;
	kstat.physical_reserved <<= 2;

	printk("\n");
	printk("memory: total=%dKB, user=%dKB, kernel=%dKB, reserved=%dKB\n",
		kstat.physical_pages << 2,
		kstat.total_mem_pages << 2,
		kstat.kernel_reserved, kstat.physical_reserved);
	printk("tables: procs=%d (%dKB), opens=%d (%dKB), pages=%dKB, inodes=%d\n",
		NR_PROCS, proc_table_size / 1024,
		NR_OPENS, fd_table_size / 1024,
		page_table_size / 1024,
		kstat.max_inodes);
	printk("hash tables: buffers=%d (%dKB), inodes=%d (%dKB), pages=%d (%dKB)\n",
		buffer_hash_table_size / sizeof(*buffer_hash_table), buffer_hash_table_size / 1024,
		inode_hash_table_size / sizeof(*inode_hash_table), inode_hash_table_size / 1024,
		page_hash_table_size / sizeof(*page_hash_table), page_hash_table_size / 1024);
	printk("kernel: text=%dKB, data=%dKB, bss=%dKB\n\n",
		KERNEL_TEXT_SIZE / 1024, KERNEL_DATA_SIZE / 1024, KERNEL_BSS_SIZE / 1024);
}
