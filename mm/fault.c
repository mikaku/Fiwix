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

static int page_protection_violation(struct vma *vma, __addr_t address)
{
	int status;

	status = copy_on_write_page(vma, address);
	if(status < 0) {
		return PFAULT_SIGSEGV;
	}
	return status ? PFAULT_SIGKILL : PFAULT_RESOLVED;
}

static struct vma *find_stack_region(void)
{
	struct vma *vma;

	vma = current->vma_table;
	while(vma) {
		if(vma->s_type == P_STACK) {
			return vma;
		}
		vma = vma->next;
	}
	return NULL;
}

static int page_not_present(struct vma *vma, __addr_t address,
	__addr_t user_sp)
{
	unsigned int file_offset;
	__addr_t addr;
	struct page *pg;

	if(!vma) {
		if(user_sp >= 32 && address >= (user_sp - 32) &&
			address < PAGE_OFFSET) {
			if(!(vma = find_stack_region())) {
				printk("WARNING: %s(): process %d doesn't have an stack region in vma_table!\n", __FUNCTION__, current->pid);
				return PFAULT_SIGSEGV;
			} else {
				if(vma != current->vma_table &&
					(address & PAGE_MASK) < vma->prev->end) {
					return PFAULT_SIGSEGV;
				}
				/* assuming stack will never reach heap */
				vma->start = address;
				vma->start = vma->start & PAGE_MASK;
			}
		}
	}

	/* if still a non-valid vma is found then kill the process! */
	if(!vma || vma->prot == PROT_NONE) {
		return PFAULT_SIGSEGV;
	}

	/* fill the page with its corresponding file content */
	if(vma->inode) {
		file_offset = (address & PAGE_MASK) - vma->start + vma->offset;
		file_offset &= PAGE_MASK;
		pg = NULL;

		if(!(vma->prot & PROT_WRITE) || vma->flags & MAP_SHARED) {
			/* check if it's already in cache */
			if((pg = search_page_hash(vma->inode, file_offset))) {
				if(!map_page(current, address, (__addr_t)V2P(pg->data), vma->prot)) {
					printk("%s(): Oops, map_page() returned 0!\n", __FUNCTION__);
					return PFAULT_SIGKILL;
				}
				page_lock(pg);
				addr = (__addr_t)pg->data;
				page_unlock(pg);
			}
		}
		if(!pg) {
			if(!(addr = map_page(current, address, 0, vma->prot))) {
				printk("%s(): Oops, map_page() returned 0!\n", __FUNCTION__);
				return PFAULT_SIGKILL;
			}
			pg = &page_table[PHYS_TO_PAGE(V2P(addr))];
			if(bread_page(pg, vma->inode, file_offset, vma->prot, vma->flags)) {
				unmap_page(address);
				return PFAULT_SIGKILL;
			}
			current->usage.ru_majflt++;
		}
	} else {
		current->usage.ru_minflt++;
		addr = 0;
#ifdef CONFIG_SYSVIPC
		if(vma->s_type == P_SHM) {
			if(shm_map_page(vma, address)) {
				return PFAULT_SIGKILL;
			}
		}
#endif /* CONFIG_SYSVIPC */
	}

	if(vma->flags & ZERO_PAGE) {
		if(!addr) {
			if(!(addr = map_page(current, address, 0, vma->prot))) {
				printk("%s(): Oops, map_page() returned 0!\n", __FUNCTION__);
				return PFAULT_SIGKILL;
			}
		}
		memset_b((void *)(addr & PAGE_MASK), 0, PAGE_SIZE);
	}

	return PFAULT_RESOLVED;
}

int resolve_page_fault(__addr_t address, unsigned int flags,
	__addr_t user_sp)
{
	struct vma *vma;

	vma = find_vma_region(address);
	if(vma) {
		if(flags & PFAULT_U) {
			if(flags & PFAULT_V) {
				if(flags & PFAULT_W) {
					return page_protection_violation(vma, address);
				}
				return PFAULT_SIGSEGV;
			}
			return page_not_present(vma, address, user_sp);
		}
		if(!(flags & PFAULT_V)) {
			return page_not_present(vma, address, user_sp);
		}
		if(flags & PFAULT_W) {
			return page_protection_violation(vma, address);
		}
		return PFAULT_FATAL;
	}

	if(flags & PFAULT_U) {
		if(flags & PFAULT_V) {
			return PFAULT_SIGSEGV;
		}
		return page_not_present(NULL, address, user_sp);
	}
	if(!(flags & PFAULT_V) && user_sp >= 32 &&
		address >= user_sp - 32 && address < PAGE_OFFSET) {
		return page_not_present(NULL, address, user_sp);
	}
	return PFAULT_FATAL;
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
	__addr_t cr2;
	__addr_t user_sp;
	struct sigcontext *usc;
	int panic, result;

	GET_CR2(cr2);
	user_sp = sc->oldesp;
	if(!(sc->err & PFAULT_U) && !find_vma_region(cr2)) {
		usc = (struct sigcontext *)((__addr_t)sc->esp +
			16 * sizeof(unsigned int));
		usc += 1;
		user_sp = usc->oldesp;
	}
	result = resolve_page_fault(cr2, sc->err, user_sp);
	if(result == PFAULT_RESOLVED) {
		return;
	}
	if(result == PFAULT_SIGSEGV) {
		send_sigsegv(sc);
		return;
	}
	if(result == PFAULT_SIGKILL) {
		send_sig(current, SIGKILL);
		if(!(sc->err & PFAULT_U)) {
			printk("%s(): kernel was unable to %s a page of process '%s' (pid %d).\n",
				__FUNCTION__, sc->err & PFAULT_W ? "write" : "read",
				current->argv0, current->pid);
		}
		return;
	}

	panic = dump_registers(trap, sc);
	show_vma_regions(current);
	if(panic) {
		PANIC("");
	}
	do_exit(SIGTERM);
}
