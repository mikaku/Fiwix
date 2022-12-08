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
#include <fiwix/shm.h>

void merge_vma_regions(struct vma *, struct vma *);

void show_vma_regions(struct proc *p)
{
	__ino_t inode;
	int major, minor;
	char *section;
	char r, w, x, f;
	struct vma *vma;
	unsigned int n;
	int count;

	vma = p->vma_table;
	n = 0;
	printk("num  address range         flag offset     dev   inode      mod section cnt\n");
	printk("---- --------------------- ---- ---------- ----- ---------- --- ------- ----\n");

	while(vma) {
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
#ifdef CONFIG_SYSVIPC
			case P_SHM:	section = "shm  ";
					break;
#endif /* CONFIG_SYSVIPC */
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
		vma = vma->next;
		n++;
	}
	if(!n) {
		printk("[no vma regions]\n");
	}
}

/* insert a vma structure into vma_table sorted by address */
static void insert_vma_region(struct vma *vma)
{
	struct vma *vmat;

	vmat = current->vma_table;

	while(vmat) {
		if(vmat->start > vma->start) {
			break;
		}
		vmat = vmat->next;
	}

	if(!vmat) {
		/* append */
		vma->prev = current->vma_table->prev;
		current->vma_table->prev->next = vma;
		current->vma_table->prev = vma;
	} else {
		/* insert */
		vma->prev = vmat->prev;
		vma->next = vmat;
		vmat->prev->next = vma;
		vmat->prev = vma;
	}

	if(vma->prev->start == vma->start || vma->prev->end == vma->end) {
		merge_vma_regions(vma->prev, vma);
	}
}

static void add_vma_region(struct vma *vma)
{
	unsigned long int flags;

	SAVE_FLAGS(flags); CLI();
	if(!current->vma_table) {
		current->vma_table = vma;
		current->vma_table->prev = vma;
	} else {
		insert_vma_region(vma);
	}
	RESTORE_FLAGS(flags);
}

static void del_vma_region(struct vma *vma)
{
	unsigned long int flags;
	struct vma *tmp;

	tmp = vma;

	if(!vma->next && !vma->prev) {
		printk("WARNING: %s(): trying to delete an unexistent vma region (%x).\n", __FUNCTION__, vma->start);
		return;
	}

	SAVE_FLAGS(flags); CLI();
	if(vma->next) {
		vma->next->prev = vma->prev;
	}
	if(vma->prev) {
		if(vma != current->vma_table) {
			vma->prev->next = vma->next;
		}
	}
	if(!vma->next) {
		current->vma_table->prev = vma->prev;
	}
	if(vma == current->vma_table) {
		current->vma_table = vma->next;
	}
	RESTORE_FLAGS(flags);

	kfree2((unsigned int)tmp);
}

static int can_be_merged(struct vma *a, struct vma *b)
{
	if((a->end == b->start) &&
	   (a->prot == b->prot) &&
	   (a->flags == b->flags) &&
	   (a->offset == b->offset) &&
	   (a->s_type == b->s_type) &&
#ifdef CONFIG_SYSVIPC
	   (a->s_type != P_SHM) &&
#endif /* CONFIG_SYSVIPC */
	   (a->inode == b->inode)) {
		return 1;
	}

	return 0;
}

/* this assumes that vma_table is sorted by address */
void merge_vma_regions(struct vma *a, struct vma *b)
{
	struct vma *new;

	if(b->start == a->end) {
		if(can_be_merged(a, b)) {
			a->end = b->end;
			kfree2((unsigned int)b);
			return;
		}
	}

	if((b->start < a->end)) {
		if(!(new = (struct vma *)kmalloc2(sizeof(struct vma)))) {
			return;
		}
		new->start = b->end;
		new->end = a->end;
		new->prot = a->prot;
		new->flags = a->flags;
		new->offset = a->offset;
		new->s_type = a->s_type;
		new->inode = a->inode;
		new->o_mode = a->o_mode;
		a->end = b->start;
		if(a->start == a->end) {
			del_vma_region(a);
		}
		if(new->start == new->end) {
			kfree2((unsigned int)new);
		}
	}
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

	if(start + length < vma->end) {
		if(!(new = (struct vma *)kmalloc2(sizeof(struct vma)))) {
			return -ENOMEM;
		}
		memset_b(new, 0, sizeof(struct vma));
		new->start = start + length;
		new->end = vma->end;
		new->prot = vma->prot;
		new->flags = vma->flags;
		new->offset = vma->offset;
		new->s_type = vma->s_type;
		new->inode = vma->inode;
		new->o_mode = vma->o_mode;
	} else {
		new = NULL;
	}

	if(vma->start == start) {
		if(vma->inode) {
			iput(vma->inode);
		}
		del_vma_region(vma);
	} else {
		vma->end = start;
	}

	if(new) {
		add_vma_region(new);
	}

	return 0;
}

void release_binary(void)
{
	struct vma *vma, *tmp;

	vma = current->vma_table;

	while(vma) {
		tmp = vma->next;
		free_vma_pages(vma->start, vma->end - vma->start, vma);
		free_vma_region(vma, vma->start, vma->end - vma->start);
		vma = tmp;
	}

	invalidate_tlb();
}

struct vma *find_vma_region(unsigned int addr)
{
	struct vma *vma;

	if(!addr) {
		return NULL;
	}

	addr &= PAGE_MASK;
	vma = current->vma_table;

	while(vma) {
		if((addr >= vma->start) && (addr < vma->end)) {
			return vma;
		}
		vma = vma->next;
	}
	return NULL;
}

struct vma *find_vma_intersection(unsigned int start, unsigned int end)
{
	struct vma *vma;

	vma = current->vma_table;

	while(vma) {
		if(end <= vma->start) {
			break;
		}
		if(start < vma->end) {
			return vma;
		}
		vma = vma->next;
	}
	return NULL;
}

int expand_heap(unsigned int new)
{
	struct vma *vma, *heap;

	vma = current->vma_table;
	heap = NULL;

	while(vma) {
		/* make sure the new heap won't overlap the next region */
		if(heap && new < vma->start) {
			heap->end = new;
			return 0;
		} else {
			heap = NULL;	/* was a bad candidate */
		}
		if(!heap && vma->s_type == P_HEAP) {
			heap = vma;	/* possible candidate */
		}
		vma = vma->next;
	}

	/* out of memory! */
	return 1;
}

/* return the first free address that matches with the size of length */
unsigned int get_unmapped_vma_region(unsigned int length)
{
	unsigned int addr;
	struct vma *vma;

	if(!length) {
		return 0;
	}

	addr = MMAP_START;
	vma = current->vma_table;

	while(vma) {
		if(vma->start < MMAP_START) {
			vma = vma->next;
			continue;
		}
		if(vma->start - addr >= length) {
			return PAGE_ALIGN(addr);
		}
		addr = PAGE_ALIGN(vma->end);
		vma = vma->next;
	}
	return 0;
}

int do_mmap(struct inode *i, unsigned int start, unsigned int length, unsigned int prot, unsigned int flags, unsigned int offset, char type, char mode, void *object)
{
	struct vma *vma;
	int errno;

	if(!(length = PAGE_ALIGN(length))) {
		return start;
	}

	if(start > PAGE_OFFSET || start + length > PAGE_OFFSET) {
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
#ifdef CONFIG_SYSVIPC
		/* ... except for SHM regions */
		if(type == P_SHM) {
			flags &= ~ZERO_PAGE;
		}
#endif /* CONFIG_SYSVIPC */
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

	if(!(vma = (struct vma *)kmalloc2(sizeof(struct vma)))) {
                return -ENOMEM;
        }
        memset_b(vma, 0, sizeof(struct vma));
	vma->start = start;
	vma->end = start + length;
	vma->prot = prot;
	vma->flags = flags;
	vma->offset = offset;
	vma->s_type = type;
	vma->inode = i;
	vma->o_mode = mode;
#ifdef CONFIG_SYSVIPC
	vma->object = (struct shmid_ds *)object;
#endif /* CONFIG_SYSVIPC */

	if(i && i->fsop->mmap) {
		if((errno = i->fsop->mmap(i, vma))) {
			int errno2;

			if((errno2 = free_vma_region(vma, start, length))) {
				kfree2((unsigned int)vma);
				return errno2;
			}
			kfree2((unsigned int)vma);
			return errno;
		}
	}

	add_vma_region(vma);
	return start;
}

int do_munmap(unsigned int addr, __size_t length)
{
	struct vma *vma;
	unsigned int size;

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
			free_vma_region(vma, addr, size);
			length -= size;
			addr += size;
		}
	}

	return 0;
}

int do_mprotect(struct vma *vma, unsigned int addr, __size_t length, int prot)
{
	struct vma *new;

	if(!(new = (struct vma *)kmalloc2(sizeof(struct vma)))) {
                return -ENOMEM;
        }
        memset_b(new, 0, sizeof(struct vma));
	new->start = addr;
	new->end = addr + length;
	new->prot = prot;
	new->flags = vma->flags;
	new->offset = vma->offset;
	new->s_type = vma->s_type;
	new->inode = vma->inode;
	new->o_mode = vma->o_mode;
	add_vma_region(new);

	return 0;
}
