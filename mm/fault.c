/*
 * fiwix/mm/fault.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/sigcontext.h>
#include <fiwix/asm.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/traps.h>
#include <fiwix/sched.h>
#include <fiwix/fs.h>
#include <fiwix/mman.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/syscalls.h>
#include <fiwix/shm.h>

/* send the SIGSEGV signal to the ofending process */
static void send_sigsegv(struct sigcontext *sc)
{
#if defined(CONFIG_VERBOSE_SEGFAULTS)
	dump_registers(14, sc);
	printk("Memory map:\n");
	show_vma_regions(current);
#endif /* CONFIG_VERBOSE_SEGFAULTS */
	send_sig(current, SIGSEGV);
}

static int page_protection_violation(struct vma *vma, unsigned int cr2, struct sigcontext *sc)
{
	unsigned int *pgdir;
	unsigned int *pgtbl;
	unsigned int pde, pte, addr;
	struct page *pg;
	int page;

	pde = GET_PGDIR(cr2);
	pte = GET_PGTBL(cr2);
	pgdir = (unsigned int *)P2V(current->tss.cr3);
	pgtbl = (unsigned int *)P2V((pgdir[pde] & PAGE_MASK));
	page = (pgtbl[pte] & PAGE_MASK) >> PAGE_SHIFT;

	pg = &page_table[page];

	/* Copy On Write feature */
	if(pg->count > 1) {
		/* a page not marked as copy-on-write means it's read-only */
		if(!(pg->flags & PAGE_COW)) {
			printk("Oops!, page %d NOT marked for CoW.\n", pg->page);
			send_sigsegv(sc);
			return 0;
		}
		if(!(addr = kmalloc(PAGE_SIZE))) {
			printk("%s(): not enough memory!\n", __FUNCTION__);
			return 1;
		}
		current->rss++;
		memcpy_b((void *)addr, (void *)P2V((page << PAGE_SHIFT)), PAGE_SIZE);
		pgtbl[pte] = V2P(addr) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
		kfree(P2V((page << PAGE_SHIFT)));
		current->rss--;
		invalidate_tlb();
		return 0;
	} else {
		/* last page of Copy On Write procedure */
		if(pg->count == 1) {
			/* a page not marked as copy-on-write means it's read-only */
			if(!(pg->flags & PAGE_COW)) {
				printk("Oops!, last page %d NOT marked for CoW.\n", pg->page);
				send_sigsegv(sc);
				return 0;
			}
			pgtbl[pte] = (page << PAGE_SHIFT) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
			invalidate_tlb();
			return 0;
		}
	}
	printk("WARNING: %s(): page %d with pg->count = 0!\n", __FUNCTION__, pg->page);
	return 1;
}

static int page_not_present(struct vma *vma, unsigned int cr2, struct sigcontext *sc)
{
	unsigned int addr, file_offset;
	struct page *pg;

	if(!vma) {
		if(cr2 >= (sc->oldesp - 32) && cr2 < PAGE_OFFSET) {
			if(!(vma = find_vma_region(PAGE_OFFSET - 1))) {
				printk("WARNING: %s(): process %d doesn't have an stack region in vma_table!\n", __FUNCTION__, current->pid);
				send_sigsegv(sc);
				return 0;
			} else {
				/* assuming stack will never reach heap */
				vma->start = cr2;
				vma->start = vma->start & PAGE_MASK;
			}
		}
	}

	/* if still a non-valid vma is found then kill the process! */
	if(!vma || vma->prot == PROT_NONE) {
		send_sigsegv(sc);
		return 0;
	}

	/* fill the page with its corresponding file content */
	if(vma->inode) {
		file_offset = (cr2 & PAGE_MASK) - vma->start + vma->offset;
		file_offset &= PAGE_MASK;
		pg = NULL;

		if(!(vma->prot & PROT_WRITE) || vma->flags & MAP_SHARED) {
			/* check if it's already in cache */
			if((pg = search_page_hash(vma->inode, file_offset))) {
				if(!map_page(current, cr2, (unsigned int)V2P(pg->data), vma->prot)) {
					printk("%s(): Oops, map_page() returned 0!\n", __FUNCTION__);
					return 1;
				}
				page_lock(pg);
				addr = (unsigned int)pg->data;
				page_unlock(pg);
			}
		}
		if(!pg) {
			if(!(addr = map_page(current, cr2, 0, vma->prot))) {
				printk("%s(): Oops, map_page() returned 0!\n", __FUNCTION__);
				return 1;
			}
			pg = &page_table[V2P(addr) >> PAGE_SHIFT];
			if(bread_page(pg, vma->inode, file_offset, vma->prot, vma->flags)) {
				unmap_page(cr2);
				return 1;
			}
			current->usage.ru_majflt++;
		}
	} else {
		current->usage.ru_minflt++;
		addr = 0;
#ifdef CONFIG_SYSVIPC
		if(vma->s_type == P_SHM) {
			if(shm_map_page(vma, cr2)) {
				return 1;
			}
		}
#endif /* CONFIG_SYSVIPC */
	}

	if(vma->flags & ZERO_PAGE) {
		if(!addr) {
			if(!(addr = map_page(current, cr2, 0, vma->prot))) {
				printk("%s(): Oops, map_page() returned 0!\n", __FUNCTION__);
				return 1;
			}
		}
		memset_b((void *)(addr & PAGE_MASK), 0, PAGE_SIZE);
	}

	return 0;
}

/*
 * Exception 0xE: Page Fault
 *
 *		 +------+------+------+------+------+------+
 *		 | user |kernel|  PV  |  PF  | read |write |
 * +-------------+------+------+------+------+------+------+
 * |the page     | U1   |    K1| U1 K1|      | U1 K1|    K1|
 * |has          | U2   |    K2| U2   |    K2|    K2| U2 K2|
 * |a vma region | U3   |      |      | U3   | U3   | U3   |
 * +-------------+------+------+------+------+------+------+
 * |the page     | U1   |    K1| U1   |    K1| U1 K1| U1 K1|
 * |doesn't have | U2   |    K2|    K2| U2   | U2 K2| U2 K2|
 * |a vma region |      |      |      |      |      |      |
 * +-------------+------+------+------+------+------+------+
 *
 * U1 - vma + user + PV + read
 *	(vma page in user-mode, page-violation during read)
 *	U1.1) if flags match			-> Demand paging
 *	U1.2) if flags don't match		-> SIGSEV
 *
 * U2 - vma + user + PV + write
 *	(vma page in user-mode, page-violation during write)
 *	U2.1) if flags match			-> Copy-On-Write
 *	U2.2) if flags don't match		-> SIGSEGV
 *
 * U3 - vma + user + PF + (read | write)	-> Demand paging
 *	(vma page in user-mode, page-fault during read or write)
 *
 * K1 - vma + kernel + PV + (read | write)	-> PANIC
 *	(vma page in kernel-mode, page-violation during read or write)
 * K2 - vma + kernel + PF + (read | write)	-> Demand paging (mmap)
 *	(vma page in kernel-mode, page-fault during read or write)
 *
 * ----------------------------------------------------------------------------
 *
 * U1 - !vma + user + PV + (read | write)	-> SIGSEGV
 *	(!vma page in user-mode, page-violation during read or write)
 * U2 - !vma + user + PF + (read | write)	-> STACK?
 *	(!vma page in user-mode, page-fault during read or write)
 *
 * K1 - !vma + kernel + PF + (read | write)	-> STACK?
 *	(!vma page in kernel-mode, page-fault during read or write)
 * K2 - !vma + kernel + PV + (read | write)	-> PANIC
 *	(!vma page in kernel-mode, page-violation during read or write)
 */
void do_page_fault(unsigned int trap, struct sigcontext *sc)
{
	unsigned int cr2;
	struct vma *vma;

	GET_CR2(cr2);
	if((vma = find_vma_region(cr2))) {

		/* in user mode */
		if(sc->err & PFAULT_U) {
			if(sc->err & PFAULT_V) {	/* violation */
				if(sc->err & PFAULT_W) {
					if((page_protection_violation(vma, cr2, sc))) {
						send_sig(current, SIGKILL);
					}
					return;
				}
				send_sigsegv(sc);
			} else {			/* page not present */
				if((page_not_present(vma, cr2, sc))) {
					send_sig(current, SIGKILL);
				}
			}
			return;

		/* in kernel mode */
		} else {
			/* 
			 * WP bit marks the order: first check if the page is
			 * present, then check for protection violation.
			 */
			if(!(sc->err & PFAULT_V)) {	/* page not present */
				if((page_not_present(vma, cr2, sc))) {
					send_sig(current, SIGKILL);
					printk("%s(): kernel was unable to read a page of process '%s' (pid %d).\n", __FUNCTION__, current->argv0, current->pid);
				}
				return;
			}
			if(sc->err & PFAULT_W) {	/* copy-on-write? */
				if((page_protection_violation(vma, cr2, sc))) {
					send_sig(current, SIGKILL);
					printk("%s(): kernel was unable to write a page of process '%s' (pid %d).\n", __FUNCTION__, current->argv0, current->pid);
				}
				return;
			}
		}
	} else {
		/* in user mode */
		if(sc->err & PFAULT_U) {
			if(sc->err & PFAULT_V) {	/* violation */
				send_sigsegv(sc);
			} else {			/* stack? */
				if((page_not_present(vma, cr2, sc))) {
					send_sig(current, SIGKILL);
				}
			}
			return;

		/* in kernel mode */
		} else {
			/*
			 * When CONFIG_LAZY_USER_ADDR_CHECK is enabled, the
			 * kernel may incur in a page fault when trying to
			 * access a possible user stack address. In that case,
			 * sc->oldesp doesn't point to the user stack, but to
			 * the kernel stack, because the page fault was raised
			 * in kernel mode.
			 * We need to get the original user sigcontext struct
			 * from the current kernel stack, in order to obtain
			 * the user stack pointer sc->oldesp, and see if CR2
			 * looks like a user stack address.
			 */
			struct sigcontext *usc;

			/*
			 * Since the page fault was raised in kernel mode, the
			 * exception occurred at the same privilege level, hence
			 * the %ss and %esp registers were not saved.
			 */
			usc = (struct sigcontext *)((unsigned int *)sc->esp + 16);
			usc += 1;

			/* does it look like a user stack address? */
			if(cr2 >= (usc->oldesp - 32) && cr2 < PAGE_OFFSET) {
				if((!page_not_present(vma, cr2, usc))) {
					return;
				}
			}

			dump_registers(trap, sc);
			show_vma_regions(current);
			do_exit(SIGTERM);
		}
	}

	dump_registers(trap, sc);
	show_vma_regions(current);
	PANIC("\n");
}
