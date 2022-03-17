/*
 * fiwix/mm/mmap.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/mm.h>
#include <fiwix/fs.h>
#include <fiwix/fcntl.h>
#include <fiwix/stat.h>
#include <fiwix/process.h>
#include <fiwix/mman.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

void show_vma_regions(struct proc *p)
{
	__ino_t inode;
	int major, minor;
	char *section;
	char r, w, x, f;
	struct vma *vma;
	unsigned int n;
	int count;

	vma = p->vma;
	printk("num  address range         flag offset     dev   inode      mod section cnt\n");
	printk("---- --------------------- ---- ---------- ----- ---------- --- ------- ----\n");
	for(n = 0; n < VMA_REGIONS && vma->start; n++, vma++) {
		r = vma->prot & PROT_READ ? 'r' : '-';
		w = vma->prot & PROT_WRITE ? 'w' : '-';
		x = vma->prot & PROT_EXEC ? 'x' : '-';
		if(vma->flags & MAP_SHARED) {
			f = 's';
		} else if(vma->flags & MAP_PRIVATE) {
			f = 'p';
		} else {
			f = '-';
		}
		switch(vma->s_type) {
			case P_TEXT:	section = "text ";
					break;
			case P_DATA:	section = "data ";
					break;
			case P_BSS:	section = "bss  ";
					break;
			case P_HEAP:	section = "heap ";
					break;
			case P_STACK:	section = "stack";
					break;
			case P_MMAP:	section = "mmap ";
					break;
			default:
				section = NULL;
				break;
		}
		inode = major = minor = count = 0;
		if(vma->inode) {
			inode = vma->inode->inode;
			major = MAJOR(vma->inode->dev);
			minor = MINOR(vma->inode->dev);
			count = vma->inode->count;
		}
		printk("[%02d] 0x%08x-0x%08x %c%c%c%c 0x%08x %02d:%02d %- 10u <%d> [%s]  (%d)\n", n, vma->start, vma->end, r, w, x, f, vma->offset, major, minor, inode, vma->o_mode, section, count);
	}
	if(!n) {
		printk("[no vma regions]\n");
	}
}

static struct vma * get_new_vma_region(void)
{
	unsigned int n;
	struct vma *vma;

	vma = current->vma;

	for(n = 0; n < VMA_REGIONS; n++, vma++) {
		if(!vma->start && !vma->end) {
			return vma;
		}
	}
	return NULL;
}

/*
 * This sorts regions (in ascending order), merging equal regions and keeping
 * the unused ones at the end of the array.
 */
static void sort_vma(void)
{
	unsigned int n, n2, needs_sort;
	struct vma *vma, tmp;

	vma = current->vma;

	do {
		needs_sort = 0;
		for(n = 0, n2 = 1; n2 < VMA_REGIONS; n++, n2++) {
			if(vma[n].end && vma[n2].start) {
				if((vma[n].end == vma[n2].start) &&
				  (vma[n].prot == vma[n2].prot) &&
				  (vma[n].flags == vma[n2].flags) &&
				  (vma[n].offset == vma[n2].offset) &&
				  (vma[n].s_type == vma[n2].s_type) &&
				  (vma[n].inode == vma[n2].inode)) {
					vma[n].end = vma[n2].end;
					memset_b(&vma[n2], 0, sizeof(struct vma));
					needs_sort++;
				}
			}
			if((vma[n2].start && (vma[n].start > vma[n2].start)) || (!vma[n].start && vma[n2].start)) {
				memcpy_b(&tmp, &vma[n], sizeof(struct vma));
				memcpy_b(&vma[n], &vma[n2], sizeof(struct vma));
				memcpy_b(&vma[n2], &tmp, sizeof(struct vma));
				needs_sort++;
			}
		}
	} while(needs_sort);
}

/*
 * This function removes all redundant entries.
 *
 * for example, if for any reason the map looks like this:
 * [01] 0x0808e984-0x08092000 rw-p 0x00000000 0
 * [02] 0x0808f000-0x0808ffff rw-p 0x000c0000 4066
 *
 * this function converts it to this:
 * [01] 0x0808e984-0x0808f000 rw-p 0x00000000 0
 * [02] 0x0808f000-0x0808ffff rw-p 0x000c0000 4066
 * [03] 0x08090000-0x08092000 rw-p 0x00000000 0
 */
static int optimize_vma(void)
{
	unsigned int n, needs_sort;
	struct vma *vma, *prev, *new;

	for(;;) {
		needs_sort = 0;
		prev = new = NULL;
		vma = current->vma;
		for(n = 0; n < VMA_REGIONS && vma->start; n++, vma++) {
			if(!prev) {
				prev = vma;
				continue;
			}
			if((vma->start < prev->end)) {
				if(!(new = get_new_vma_region())) {
					printk("WARNING: %s(): unable to get a free vma region.\n", __FUNCTION__);
					return -ENOMEM;
				}
				new->start = vma->end;
				new->end = prev->end;
				new->prot = prev->prot;
				new->flags = prev->flags;
				new->offset = prev->offset;
				new->s_type = prev->s_type;
				new->inode = prev->inode;
				new->o_mode = prev->o_mode;
				prev->end = vma->start;
				needs_sort++;
				if(prev->start == prev->end) {
					memset_b(prev, 0, sizeof(struct vma));
				}
				if(new->start == new->end) {
					memset_b(new, 0, sizeof(struct vma));
				}
				break;
			}
			prev = vma;
		}
		if(!needs_sort) {
			break;
		}
		sort_vma();
	}

	return 0;
}

/* return the first free address that matches with the size of length */
static unsigned int get_unmapped_vma_region(unsigned int length)
{
	unsigned int n, addr;
	struct vma *vma;

	if(!length) {
		return 0;
	}

	addr = MMAP_START;
	vma = current->vma;

	for(n = 0; n < VMA_REGIONS && vma->start; n++, vma++) {
		if(vma->start < MMAP_START) {
			continue;
		}
		if(vma->start - addr >= length) {
			return PAGE_ALIGN(addr);
		}
		addr = PAGE_ALIGN(vma->end);
	}
	return 0;
}

static void free_vma_pages(unsigned int start, __size_t length, struct vma *vma)
{
	unsigned int n, addr;
	unsigned int *pgdir, *pgtbl;
	unsigned int pde, pte;
	struct page *pg;
	int page;

	pgdir = (unsigned int *)P2V(current->tss.cr3);
	pgtbl = NULL;

	for(n = 0; n < (length / PAGE_SIZE); n++) {
		pde = GET_PGDIR(start + (n * PAGE_SIZE));
		pte = GET_PGTBL(start + (n * PAGE_SIZE));
		if(pgdir[pde] & PAGE_PRESENT) {
			pgtbl = (unsigned int *)P2V((pgdir[pde] & PAGE_MASK));
			if(pgtbl[pte] & PAGE_PRESENT) {
				/* make sure to not free reserved pages */
				page = pgtbl[pte] >> PAGE_SHIFT;
				pg = &page_table[page];
				if(pg->flags & PAGE_RESERVED) {
					continue;
				}

				if(vma->prot & PROT_WRITE && vma->flags & MAP_SHARED) {
					addr = start - vma->start + vma->offset;
					write_page(pg, vma->inode, addr, length);
				}

				kfree(P2V(pgtbl[pte]) & PAGE_MASK);
				current->rss--;
				pgtbl[pte] = 0;

				/* check if a page table can be freed */
				for(pte = 0; pte < PT_ENTRIES; pte++) {
					if(pgtbl[pte] & PAGE_MASK) {
						break;
					}
				}
				if(pte == PT_ENTRIES) {
					kfree((unsigned int)pgtbl & PAGE_MASK);
					current->rss--;
					pgdir[pde] = 0;
				}
			}
		}
	}
}

static int free_vma_region(struct vma *vma, unsigned int start, __ssize_t length)
{
	struct vma *new;

	if(!(new = get_new_vma_region())) {
		printk("WARNING: %s(): unable to get a free vma region.\n", __FUNCTION__);
		return -ENOMEM;
	}

	new->start = start + length;
	new->end = vma->end;
	new->prot = vma->prot;
	new->flags = vma->flags;
	new->offset = vma->offset;
	new->s_type = vma->s_type;
	new->inode = vma->inode;
	new->o_mode = vma->o_mode;

	vma->end = start;

	if(vma->start == vma->end) {
		if(vma->inode) {
			iput(vma->inode);
		}
		memset_b(vma, 0, sizeof(struct vma));
	}
	if(new->start == new->end) {
		memset_b(new, 0, sizeof(struct vma));
	}
	return 0;
}

void release_binary(void)
{
	unsigned int n;
	struct vma *vma;

	vma = current->vma;

	for(n = 0; n < VMA_REGIONS && vma->start; n++, vma++) {
		free_vma_pages(vma->start, vma->end - vma->start, vma);
		free_vma_region(vma, vma->start, vma->end - vma->start);
	}
	sort_vma();
	optimize_vma();
	invalidate_tlb();
}

struct vma * find_vma_region(unsigned int addr)
{
	unsigned int n;
	struct vma *vma;

	if(!addr) {
		return NULL;
	}

	addr &= PAGE_MASK;
	vma = current->vma;

	for(n = 0; n < VMA_REGIONS && vma->start; n++, vma++) {
		if((addr >= vma->start) && (addr < vma->end)) {
			return vma;
		}
	}
	return NULL;
}

int expand_heap(unsigned int new)
{
	unsigned int n;
	struct vma *vma, *heap;

	vma = current->vma;
	heap = NULL;

	for(n = 0; n < VMA_REGIONS && vma->start; n++, vma++) {
		/* make sure the new heap won't overlap the next region */
		if(heap && new < vma->start) {
			heap->end = new;
			return 0;
		} else {
			heap = NULL;	/* was a bad candidate */
		}
		if(!heap && vma->s_type == P_HEAP) {
			heap = vma;	/* possible candidate */
			continue;
		}
	}

	/* out of memory! */
	return 1;
}

int do_mmap(struct inode *i, unsigned int start, unsigned int length, unsigned int prot, unsigned int flags, unsigned int offset, char type, char mode)
{
	struct vma *vma;
	int errno;

	if(!(length = PAGE_ALIGN(length))) {
		return start;
	}

	if(start > KERNEL_BASE_ADDR || start + length > KERNEL_BASE_ADDR) {
		return -EINVAL;
	}

	/* file mapping */
	if(i) {
		if(!S_ISREG(i->i_mode) && !S_ISCHR(i->i_mode)) {
			return -ENODEV;
		}

		/* 
		 * The file shall have been opened with read permission,
		 * regardless of the protection options specified.
		 * IEEE Std 1003.1, 2004 Edition.
		 */
		if(mode == O_WRONLY) {
			return -EACCES;
		}
		switch(flags & MAP_TYPE) {
			case MAP_SHARED:
				if(prot & PROT_WRITE) {
					if(!(mode & (O_WRONLY | O_RDWR))) {
						return -EACCES;
					}
				}
				break;
			case MAP_PRIVATE:
				break;
			default:
				return -EINVAL;
		}
		i->count++;

	/* anonymous mapping */
	} else {
		if((flags & MAP_TYPE) != MAP_PRIVATE) {
			return -EINVAL;
		}

		/* anonymous objects must be filled with zeros */
		flags |= ZERO_PAGE;
	}

	if(flags & MAP_FIXED) {
		if(start & ~PAGE_MASK) {
			return -EINVAL;
		}
	} else {
		start = get_unmapped_vma_region(length);
		if(!start) {
			printk("WARNING: %s(): unable to get an unmapped vma region.\n", __FUNCTION__);
			return -ENOMEM;
		}
	}

	if(!(vma = get_new_vma_region())) {
		printk("WARNING: %s(): unable to get a free vma region.\n", __FUNCTION__);
		return -ENOMEM;
	}

	vma->start = start;
	vma->end = start + length;
	vma->prot = prot;
	vma->flags = flags;
	vma->offset = offset;
	vma->s_type = type;
	vma->inode = i;
	vma->o_mode = mode;

	if(i && i->fsop->mmap) {
		if((errno = i->fsop->mmap(i, vma))) {
			int errno2;

			if((errno2 = free_vma_region(vma, start, length))) {
				return errno2;
			}
			sort_vma();
			if((errno2 = optimize_vma())) {
				return errno2;
			}
			return errno;
		}
	}

	sort_vma();
	if((errno = optimize_vma())) {
		return errno;
	}
	return start;
}

int do_munmap(unsigned int addr, __size_t length)
{
	struct vma *vma;
	unsigned int size;
	int errno;

	if(addr & ~PAGE_MASK) {
		return -EINVAL;
	}

	length = PAGE_ALIGN(length);

	while(length) {
		if((vma = find_vma_region(addr))) {
			if((addr + length) > vma->end) {
				size = vma->end - addr;
			} else {
				size = length;
			}

			free_vma_pages(addr, size, vma);
			invalidate_tlb();
			if((errno = free_vma_region(vma, addr, size))) {
				return errno;
			}
			sort_vma();
			if((errno = optimize_vma())) {
				return errno;
			}
			length -= size;
			addr += size;
		} else {
			break;
		}
	}

	return 0;
}

int do_mprotect(struct vma *vma, unsigned int addr, __size_t length, int prot)
{
	struct vma *new;
	int errno;

	if(!(new = get_new_vma_region())) {
		printk("WARNING: %s(): unable to get a free vma region.\n", __FUNCTION__);
		return -ENOMEM;
	}

	new->start = addr;
	new->end = addr + length;
	new->prot = prot;
	new->flags = vma->flags;
	new->offset = vma->offset;
	new->s_type = vma->s_type;
	new->inode = vma->inode;
	new->o_mode = vma->o_mode;

	sort_vma();
	if((errno = optimize_vma())) {
		return errno;
	}
	return 0;
}
