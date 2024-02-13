/*
 * fiwix/mm/memory.c
 *
 * Copyright 2018-2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/asm.h>
#include <fiwix/multiboot1.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/bios.h>
#include <fiwix/ramdisk.h>
#include <fiwix/process.h>
#include <fiwix/buffer.h>
#include <fiwix/fs.h>
#include <fiwix/kexec.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define KERNEL_TEXT_SIZE	((int)_etext - (PAGE_OFFSET + KERNEL_ADDR))
#define KERNEL_DATA_SIZE	((int)_edata - (int)_etext)
#define KERNEL_BSS_SIZE		((int)_end - (int)_edata)

unsigned int *kpage_dir;

unsigned int proc_table_size = 0;

unsigned int buffer_table_size = 0;
unsigned int buffer_hash_table_size = 0;

unsigned int inode_table_size = 0;
unsigned int inode_hash_table_size = 0;

unsigned int fd_table_size = 0;

unsigned int page_table_size = 0;
unsigned int page_hash_table_size = 0;

unsigned int map_kaddr(unsigned int from, unsigned int to, unsigned int addr, int flags)
{
	unsigned int n;
	unsigned int *pgtbl;
	unsigned int pde, pte;

	for(n = from; n < to; n += PAGE_SIZE) {
		pde = GET_PGDIR(n);
		pte = GET_PGTBL(n);
		if(!(kpage_dir[pde] & ~PAGE_MASK)) {
			kpage_dir[pde] = addr | flags;
			memset_b((void *)addr, 0, PAGE_SIZE);
			addr += PAGE_SIZE;
		}
		pgtbl = (unsigned int *)(kpage_dir[pde] & PAGE_MASK);
		pgtbl[pte] = n | flags;
	}

	return addr;
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
		memksize = 4096;
	} else {
		mbi = (struct multiboot_info *)(PAGE_OFFSET + info);
		if(!(mbi->flags & MULTIBOOT_INFO_MEMORY)) {
			/* 4MB of memory assumed */
			memksize = 4096;
		} else {
			/* we need to add the first 1MB to memksize */
			memksize = (unsigned int)mbi->mem_upper + 1024;
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

	for(n = 0; n < memksize / sizeof(unsigned int); n++) {
		pgtbl[n] = (n << PAGE_SHIFT) | PAGE_PRESENT | PAGE_RW;
		if(!(n % 1024)) {
			pd = n / 1024;
			kpage_dir[pd] = (unsigned int)(addr + (PAGE_SIZE * pd) + GDT_BASE) | PAGE_PRESENT | PAGE_RW;
			kpage_dir[GET_PGDIR(PAGE_OFFSET) + pd] = (unsigned int)(addr + (PAGE_SIZE * pd) + GDT_BASE) | PAGE_PRESENT | PAGE_RW;
		}
	}
	return (unsigned int)kpage_dir + GDT_BASE;
}

/* returns the mapped address of a virtual address */
unsigned int get_mapped_addr(struct proc *p, unsigned int addr)
{
	unsigned int *pgdir, *pgtbl;
	unsigned int pde, pte;

	pgdir = (unsigned int *)P2V(p->tss.cr3);
	pde = GET_PGDIR(addr);
	pte = GET_PGTBL(addr);
	pgtbl = (unsigned int *)P2V((pgdir[pde] & PAGE_MASK));
	return pgtbl[pte];
}

int clone_pages(struct proc *child)
{
	unsigned int *src_pgdir, *dst_pgdir;
	unsigned int *src_pgtbl, *dst_pgtbl;
	unsigned int pde, pte;
	unsigned int p_addr, c_addr;
	unsigned int n, pages;
	struct page *pg;
	struct vma *vma;

	src_pgdir = (unsigned int *)P2V(current->tss.cr3);
	dst_pgdir = (unsigned int *)P2V(child->tss.cr3);
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
					p_addr = src_pgtbl[pte] >> PAGE_SHIFT;
					pg = &page_table[p_addr];
					if(pg->flags & PAGE_RESERVED) {
						continue;
					}
					src_pgtbl[pte] &= ~PAGE_RW;
					/* mark write-only pages as copy-on-write */
					if(vma->prot & PROT_WRITE) {
						pg->flags |= PAGE_COW;
					}
					dst_pgtbl[pte] = src_pgtbl[pte];
					if(!is_valid_page((dst_pgtbl[pte] & PAGE_MASK) >> PAGE_SHIFT)) {
						PANIC("%s: missing page %d during copy-on-write process.\n", __FUNCTION__, (dst_pgtbl[pte] & PAGE_MASK) >> PAGE_SHIFT);
					}
					pg = &page_table[(dst_pgtbl[pte] & PAGE_MASK) >> PAGE_SHIFT];
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

	pgdir = (unsigned int *)P2V(p->tss.cr3);
	for(n = 0, count = 0; n < PD_ENTRIES; n++) {
		if((pgdir[n] & (PAGE_PRESENT | PAGE_RW | PAGE_USER)) == (PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
			kfree(P2V(pgdir[n]) & PAGE_MASK);
			pgdir[n] = 0;
			count++;
		}
	}
	return count;
}

unsigned int map_page(struct proc *p, unsigned int vaddr, unsigned int addr, unsigned int prot)
{
	unsigned int *pgdir, *pgtbl;
	unsigned int newaddr;
	int pde, pte;

	pgdir = (unsigned int *)P2V(p->tss.cr3);
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
		pgtbl[pte] = addr | PAGE_PRESENT | PAGE_USER;
	}
	if(prot & PROT_WRITE) {
		pgtbl[pte] |= PAGE_RW;
	}
	return P2V(addr);
}

int unmap_page(unsigned int vaddr)
{
	unsigned int *pgdir, *pgtbl;
	unsigned int addr;
	int pde, pte;

	pgdir = (unsigned int *)P2V(current->tss.cr3);
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

	addr = pgtbl[pte] & PAGE_MASK;
	pgtbl[pte] = 0;
	kfree(P2V(addr));
	current->rss--;
	return 0;
}

/*
 * This function initializes and setups the kernel page directory and page
 * tables. It also reserves areas of contiguous memory spaces for internal
 * structures and for the RAMdisk drives.
 */
void mem_init(void)
{
	unsigned int sizek;
	unsigned int physical_memory, physical_page_tables;
	unsigned int *pgtbl;
	int n, pages, last_ramdisk;

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

#ifdef CONFIG_KEXEC
	if(kexec_size > 0) {
		bios_map_reserve(KEXEC_BOOT_ADDR, KEXEC_BOOT_ADDR + (PAGE_SIZE * 2) - 1);
		_last_data_addr = map_kaddr(KEXEC_BOOT_ADDR, KEXEC_BOOT_ADDR + (PAGE_SIZE * 2), _last_data_addr, PAGE_PRESENT | PAGE_RW);
	}
#endif /* CONFIG_KEXEC */

	_last_data_addr = map_kaddr(0xA0000, 0xA0000 + video.memsize, _last_data_addr, PAGE_PRESENT | PAGE_RW);

	/*
	 * FIXME:
	 * Why do I need to reserve the page tables for video framebuffer
	 * here, instead of using kmalloc() in fbcon_init() and bga_init()?
	 */
	video.pgtbl_addr = _last_data_addr;
	if(video.flags & VPF_VESAFB) {
		/* reserve 4 page tables (16MB) */
		_last_data_addr += (PAGE_SIZE * 4);
	}

	/* two steps mapping to make sure not include an initrd image */
	_last_data_addr = map_kaddr(KERNEL_ADDR, ((unsigned int)_end & PAGE_MASK) - PAGE_OFFSET + PAGE_SIZE, _last_data_addr, PAGE_PRESENT | PAGE_RW);
	_last_data_addr = map_kaddr((unsigned int)kpage_dir, _last_data_addr, _last_data_addr, PAGE_PRESENT | PAGE_RW);
	activate_kpage_dir();

	/* since Page Directory is now activated we can use virtual addresses */
	_last_data_addr = P2V(_last_data_addr);


	/* reserve memory space for proc_table[NR_PROCS] */
	proc_table_size = PAGE_ALIGN(sizeof(struct proc) * NR_PROCS);
	if(!is_addr_in_bios_map(V2P(_last_data_addr) + proc_table_size)) {
		PANIC("Not enough memory for proc_table.\n");
	}
	proc_table = (struct proc *)_last_data_addr;
	_last_data_addr += proc_table_size;


	/* reserve memory space for buffer_table */
	buffer_table_size = (kstat.physical_pages * BUFFER_PERCENTAGE) / 100;
	buffer_table_size *= sizeof(struct buffer);
	pages = buffer_table_size >> PAGE_SHIFT;
	buffer_table_size = !pages ? 4096 : pages << PAGE_SHIFT;

	/* reserve memory space for buffer_hash_table */
	kstat.max_buffers = buffer_table_size / sizeof(struct buffer);
	n = (kstat.max_buffers * BUFFER_HASH_PERCENTAGE) / 100;
	n = MAX(n, 10);	/* 10 buffer hashes as minimum */
	/* buffer_hash_table is an array of pointers */
	pages = ((n * sizeof(unsigned int)) / PAGE_SIZE) + 1;
	buffer_hash_table_size = pages << PAGE_SHIFT;
	if(!is_addr_in_bios_map(V2P(_last_data_addr) + buffer_hash_table_size)) {
		PANIC("Not enough memory for buffer_hash_table.\n");
	}
	buffer_hash_table = (struct buffer **)_last_data_addr;
	_last_data_addr += buffer_hash_table_size;


	/* reserve memory space for inode_hash_table */
	sizek = physical_memory / 1024;	/* this helps to avoid overflow */
	inode_table_size = (sizek * INODE_PERCENTAGE) / 100;
	inode_table_size *= 1024;
	pages = inode_table_size >> PAGE_SHIFT;
	inode_table_size = pages << PAGE_SHIFT;

	kstat.max_inodes = inode_table_size / sizeof(struct inode);
	n = (kstat.max_inodes * INODE_HASH_PERCENTAGE) / 100;
	n = MAX(n, 10);	/* 10 inode hash buckets as minimum */
	/* inode_hash_table is an array of pointers */
	pages = ((n * sizeof(unsigned int)) / PAGE_SIZE) + 1;
	inode_hash_table_size = pages << PAGE_SHIFT;
	if(!is_addr_in_bios_map(V2P(_last_data_addr) + inode_hash_table_size)) {
		PANIC("Not enough memory for inode_hash_table.\n");
	}
	inode_hash_table = (struct inode **)_last_data_addr;
	_last_data_addr += inode_hash_table_size;


	/* reserve memory space for fd_table[NR_OPENS] */
	fd_table_size = PAGE_ALIGN(sizeof(struct fd) * NR_OPENS);
	if(!is_addr_in_bios_map(V2P(_last_data_addr) + fd_table_size)) {
		PANIC("Not enough memory for fd_table.\n");
	}
	fd_table = (struct fd *)_last_data_addr;
	_last_data_addr += fd_table_size;


	/* reserve memory space for RAMdisk drives */
	last_ramdisk = 0;
	if(kparm_ramdisksize > 0 || ramdisk_table[0].addr) {
		/*
		 * If the 'initrd=' parameter was supplied, then the first
		 * RAMdisk drive was already assigned to the initrd image.
		 */
		if(ramdisk_table[0].addr) {
			ramdisk_table[0].addr += PAGE_OFFSET;
			last_ramdisk = 1;
		}
		for(; last_ramdisk < ramdisk_minors; last_ramdisk++) {
			if(!is_addr_in_bios_map(V2P(_last_data_addr) + (kparm_ramdisksize * 1024))) {
				kparm_ramdisksize = 0;
				ramdisk_minors -= RAMDISK_DRIVES;
				printk("WARNING: RAMdisk drive disabled (not enough physical memory).\n");
				break;
			}
			ramdisk_table[last_ramdisk].addr = (char *)_last_data_addr;
			ramdisk_table[last_ramdisk].size = kparm_ramdisksize;
			_last_data_addr += kparm_ramdisksize * 1024;
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
		ramdisk_minors++;
		if(last_ramdisk < ramdisk_minors) {
			if(!is_addr_in_bios_map(V2P(_last_data_addr) + (kexec_size * 1024))) {
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
	n = (kstat.physical_pages * PAGE_HASH_PERCENTAGE) / 100;
	n = MAX(n, 1);	/* 1 page for the hash table as minimum */
	n = MIN(n, 16);	/* 16 pages for the hash table as maximum */
	page_hash_table_size = n * PAGE_SIZE;
	if(!is_addr_in_bios_map(V2P(_last_data_addr) + page_hash_table_size)) {
		PANIC("Not enough memory for page_hash_table.\n");
	}
	page_hash_table = (struct page **)_last_data_addr;
	_last_data_addr += page_hash_table_size;

	page_table_size = PAGE_ALIGN(kstat.physical_pages * sizeof(struct page));
	if(!is_addr_in_bios_map(V2P(_last_data_addr) + page_table_size)) {
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
	printk("hash table entries: buffer=%d (%dKB), inode=%d (%dKB), page=%d (%dKB)\n",
		buffer_hash_table_size / sizeof(unsigned int), buffer_hash_table_size / 1024,
		inode_hash_table_size / sizeof(unsigned int), inode_hash_table_size / 1024,
		page_hash_table_size / sizeof(unsigned int), page_hash_table_size / 1024);
	printk("kernel: text=%dKB, data=%dKB, bss=%dKB, i/o buffers=%d, inodes=%d\n\n",
		KERNEL_TEXT_SIZE / 1024, KERNEL_DATA_SIZE / 1024, KERNEL_BSS_SIZE / 1024,
		buffer_table_size / sizeof(struct buffer),
		inode_table_size / sizeof(struct inode));
}
