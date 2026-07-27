/*
 * fiwix/arch/arm/memory.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/arm_vm.h>
#include <fiwix/kernel.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/process.h>
#include <fiwix/shm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define ARM_L1_TYPE_MASK		0x00000003U
#define ARM_L1_COARSE_ADDRESS		0xFFFFFC00U
#define ARM_PAGE_ADDRESS		0xFFFFF000U
#define ARM_PAGE_PERMISSION_MASK	0x00000230U
#define ARM_PAGE_USER_RO		0x00000020U
#define ARM_PAGE_USER_RW		0x00000030U

static int arm_page_present(unsigned int descriptor)
{
	return descriptor & 0x00000002U;
}

static int arm_page_writable(unsigned int descriptor)
{
	return (descriptor & ARM_PAGE_PERMISSION_MASK) == ARM_PAGE_USER_RW;
}

static unsigned int arm_page_descriptor(__addr_t physical,
	unsigned int prot)
{
	unsigned int flags;

	if(physical < ARM_VM_RAM_BASE ||
		physical >= ARM_VM_IDENTITY_LIMIT ||
		physical & (PAGE_SIZE - 1) ||
		!(prot & (PROT_READ | PROT_WRITE | PROT_EXEC)) ||
		prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) {
		return 0;
	}
	if(prot & PROT_WRITE) {
		flags = prot & PROT_EXEC ?
			ARM_VM_PAGE_USER_RW : ARM_VM_PAGE_USER_RW_XN;
	} else {
		flags = prot & PROT_EXEC ?
			ARM_VM_PAGE_USER_RO : ARM_VM_PAGE_USER_RO_XN;
	}
	return physical | flags;
}

static unsigned int *arm_walk(struct proc *p, __addr_t address, int create)
{
	unsigned int descriptor;
	unsigned int *root;
	unsigned int *table;
	__addr_t page;

	if(address < ARM_VM_USER_BASE || address >= ARM_VM_USER_LIMIT) {
		return 0;
	}
	root = arm_process_root(p);
	if(!root) {
		return 0;
	}
	descriptor = root[address >> 20];
	if(!descriptor) {
		if(!create || !(page = kmalloc(PAGE_SIZE))) {
			return 0;
		}
		memset_b((void *)(unsigned long)page, 0, PAGE_SIZE);
		if(arm_vm_attach_user_table(root, address & 0xFFF00000U,
			V2P(page))) {
			kfree(page);
			return 0;
		}
		p->rss++;
		descriptor = root[address >> 20];
	} else if((descriptor & ARM_L1_TYPE_MASK) != ARM_VM_COARSE_TABLE) {
		return 0;
	}
	table = (unsigned int *)(unsigned long)P2V(
		descriptor & ARM_L1_COARSE_ADDRESS);
	return &table[(address >> PAGE_SHIFT) & 0xFFU];
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
	memset_b(_edata, 0, (unsigned long)_end - (unsigned long)_edata);
}

unsigned int setup_tmp_pgdir(unsigned int magic, unsigned int info)
{
	(void)magic;
	(void)info;
	return 0;
}

__addr_t get_mapped_addr(struct proc *p, __addr_t addr)
{
	unsigned int descriptor;
	unsigned int *entry;

	entry = arm_walk(p, addr, 0);
	if(!entry || !arm_page_present(*entry)) {
		return 0;
	}
	descriptor = (*entry & ARM_PAGE_ADDRESS) |
		PAGE_PRESENT | PAGE_USER;
	if(arm_page_writable(*entry)) {
		descriptor |= PAGE_RW;
	}
	return descriptor;
}

int copy_on_write_page(struct vma *vma, __addr_t addr)
{
	unsigned int descriptor;
	unsigned int *entry;
	__addr_t newaddr;
	__addr_t physical;
	struct page *pg;
	int page;

	(void)vma;
	entry = arm_walk(current, addr, 0);
	if(!entry || !arm_page_present(*entry)) {
		return 1;
	}
	physical = *entry & ARM_PAGE_ADDRESS;
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
		memcpy_b((void *)(unsigned long)newaddr,
			(void *)(unsigned long)P2V(physical), PAGE_SIZE);
		descriptor = *entry & ~ARM_PAGE_ADDRESS;
		descriptor &= ~ARM_PAGE_PERMISSION_MASK;
		*entry = V2P(newaddr) | descriptor | ARM_PAGE_USER_RW;
		kfree(P2V(physical));
		invalidate_tlb();
		return 0;
	}
	if(pg->count == 1) {
		*entry &= ~ARM_PAGE_PERMISSION_MASK;
		*entry |= ARM_PAGE_USER_RW;
		invalidate_tlb();
		return 0;
	}
	return 1;
}

int clone_pages(struct proc *child)
{
	unsigned int *destination;
	unsigned int *source;
	__addr_t address;
	__addr_t physical;
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
			source = arm_walk(current, address, 0);
			if(!source || !arm_page_present(*source)) {
				continue;
			}
			destination = arm_walk(child, address, 1);
			if(!destination) {
				return 0;
			}
			physical = *source & ARM_PAGE_ADDRESS;
			if(!is_valid_page(PHYS_TO_PAGE(physical))) {
				return 0;
			}
			pg = &page_table[PHYS_TO_PAGE(physical)];
			if(pg->flags & PAGE_RESERVED) {
				continue;
			}
			*source &= ~ARM_PAGE_PERMISSION_MASK;
			*source |= ARM_PAGE_USER_RO;
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

static int arm_table_empty(const unsigned int *table)
{
	unsigned int n;

	for(n = 0; n < ARM_VM_L2_ENTRIES; n++) {
		if(arm_page_present(table[n])) {
			return 0;
		}
	}
	return 1;
}

int free_page_tables(struct proc *p)
{
	unsigned int descriptor;
	unsigned int *root;
	unsigned int *table;
	unsigned int n;
	int count;

	root = arm_process_root(p);
	if(!root) {
		return 0;
	}
	count = 0;
	for(n = ARM_VM_USER_BASE >> 20;
		n < ARM_VM_USER_LIMIT >> 20; n++) {
		descriptor = root[n];
		if((descriptor & ARM_L1_TYPE_MASK) != ARM_VM_COARSE_TABLE) {
			continue;
		}
		table = (unsigned int *)(unsigned long)P2V(
			descriptor & ARM_L1_COARSE_ADDRESS);
		if(arm_table_empty(table)) {
			kfree((__addr_t)(unsigned long)table);
			root[n] = 0;
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
	unsigned int descriptor;
	unsigned int *entry;
	__addr_t physical;

	if(flags & PAGE_NOALLOC) {
		return 0;
	}
	entry = arm_walk(p, vaddr, 1);
	if(!entry) {
		return 0;
	}
	if(!arm_page_present(*entry)) {
		physical = addr;
		if(!physical) {
			if(!(physical = kmalloc(PAGE_SIZE))) {
				return 0;
			}
			p->rss++;
			physical = V2P(physical);
		}
		descriptor = arm_page_descriptor(physical, prot);
		if(!descriptor) {
			if(!addr) {
				kfree(P2V(physical));
				p->rss--;
			}
			return 0;
		}
		*entry = descriptor;
	} else {
		physical = *entry & ARM_PAGE_ADDRESS;
		descriptor = arm_page_descriptor(physical, prot);
		if(!descriptor) {
			return 0;
		}
		*entry = descriptor;
	}
	if(p == current) {
		invalidate_tlb();
	}
	return P2V(physical);
}

int unmap_page(__addr_t vaddr)
{
	unsigned int *entry;
	__addr_t physical;

	entry = arm_walk(current, vaddr, 0);
	if(!entry || !arm_page_present(*entry)) {
		return 1;
	}
	physical = *entry & ARM_PAGE_ADDRESS;
	kfree(P2V(physical));
	*entry = 0;
	current->rss--;
	invalidate_tlb();
	return 0;
}

void free_vma_pages(struct vma *vma, __addr_t start, __size_t length)
{
	unsigned int *entry;
	unsigned int offset;
	__addr_t address;
	__addr_t physical;
	struct page *pg;

	for(address = start; address < start + length; address += PAGE_SIZE) {
		entry = arm_walk(current, address, 0);
		if(!entry || !arm_page_present(*entry)) {
			continue;
		}
		physical = *entry & ARM_PAGE_ADDRESS;
		if(is_valid_page(PHYS_TO_PAGE(physical))) {
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
