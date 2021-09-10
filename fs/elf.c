/*
 * fiwix/fs/elf.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/asm.h>
#include <fiwix/types.h>
#include <fiwix/buffer.h>
#include <fiwix/fs.h>
#include <fiwix/i386elf.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/fs.h>
#include <fiwix/fcntl.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define AT_ITEMS	12	/* ELF Auxiliary Vectors */

static int check_elf(struct elf32_hdr *elf32_h)
{
	if(elf32_h->e_ident[EI_MAG0] != ELFMAG0 ||
		elf32_h->e_ident[EI_MAG1] != ELFMAG1 ||
		elf32_h->e_ident[EI_MAG2] != ELFMAG2 ||
		elf32_h->e_ident[EI_MAG3] != ELFMAG3 ||
		(elf32_h->e_type != ET_EXEC && elf32_h->e_type != ET_DYN) ||
		elf32_h->e_machine != EM_386) {
		return -EINVAL;
	}
	return 0;
}

/*
 * Setup the initial process stack (System V ABI for i386)
 * ----------------------------------------------------------------------------
 * 0xBFFFFFFF
 * 	+---------------+	\
 * 	| envp[] str    |	|
 * 	+---------------+	|
 * 	| argv[] str    |	|
 * 	+---------------+	|
 * 	| NULL          |	|
 * 	+---------------+	|
 * 	| ELF Aux.Vect. |	|
 * 	+---------------+	|
 * 	| NULL          |	| elf_create_stack() setups this section
 * 	+---------------+	|
 * 	| envp[] ptr    |	|
 * 	+---------------+	|
 * 	| NULL          |	|
 * 	+---------------+	|
 * 	| argv[] ptr    |	|
 * 	+---------------+	|
 * 	| argc          |	|
 * 	+---------------+ 	/
 * 	| stack pointer | grows toward lower addresses
 * 	+---------------+ ||
 * 	|...............| \/
 * 	|...............|
 * 	|...............|
 * 	|...............| /\
 * 	+---------------+ ||
 * 	|  brk (heap)   | grows toward higher addresses
 * 	+---------------+
 * 	| .bss section  |
 * 	+---------------+
 * 	| .data section |
 * 	+---------------+
 * 	| .text section |
 * 	+---------------+
 * 0x08048000
 */
static void elf_create_stack(struct binargs *barg, unsigned int *sp, unsigned int str_ptr, int at_base, struct elf32_hdr *elf32_h, unsigned int phdr_addr)
{
	unsigned int n, addr;
	char *str;

	/* copy strings */
	for(n = 0; n < ARG_MAX; n++) {
		if(barg->page[n]) {
			addr = KERNEL_BASE_ADDR - ((ARG_MAX - n) * PAGE_SIZE);
			memcpy_b((void *)addr, (void *)barg->page[n], PAGE_SIZE);
		}
	}

#ifdef __DEBUG__
	printk("sp = 0x%08x\n", sp);
#endif /*__DEBUG__ */

	/* copy the value of 'argc' into the stack */
	current->argc = barg->argc;
	*sp = barg->argc;
#ifdef __DEBUG__
	printk("at 0x%08x -> argc\n", sp);
#endif /*__DEBUG__ */
	sp++;

	/* copy as many pointers to strings as 'argc' */
	current->argv = (char **)sp;
	for(n = 0; n < barg->argc; n++) {
		*sp = str_ptr;
		str = (char *)str_ptr;
#ifdef __DEBUG__
		printk("at 0x%08x -> str_ptr(%d) = 0x%08x (+ %d)\n", sp, n, str_ptr, strlen(str) + 1);
#endif /*__DEBUG__ */
		sp++;
		str_ptr += strlen(str) + 1;
	}

	/* the last element of 'argv[]' must be a NULL-pointer */
	*sp = NULL;
#ifdef __DEBUG__
	printk("at 0x%08x -> -------------- = 0x%08x\n", sp, 0);
#endif /*__DEBUG__ */
	sp++;

	/* copy as many pointers to strings as 'envc' */
	current->envc = barg->envc;
	current->envp = (char **)sp;
	for(n = 0; n < barg->envc; n++) {
		*sp = str_ptr;
		str = (char *)str_ptr;
#ifdef __DEBUG__
		printk("at 0x%08x -> str_ptr(%d) = 0x%08x (+ %d)\n", sp, n, str_ptr, strlen(str) + 1);
#endif /*__DEBUG__ */
		sp++;
		str_ptr += strlen(str) + 1;
	}

	/* the last element of 'envp[]' must be a NULL-pointer */
	*sp = NULL;
#ifdef __DEBUG__
	printk("at 0x%08x -> -------------- = 0x%08x\n", sp, 0);
#endif /*__DEBUG__ */
	sp++;


	/* copy the Auxiliar Table Items (dlinfo_items) */
	if(at_base) {
		memset_l((void *)sp, AT_PHDR, 1);
#ifdef __DEBUG__
		printk("at 0x%08x -> AT_PHDR = %d", sp, *sp);
#endif /*__DEBUG__ */
		sp++;

		memcpy_l((void *)sp, &phdr_addr, 1);
#ifdef __DEBUG__
		printk("\t\tAT_PHDR = 0x%08x\n", *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, AT_PHENT, 1);
#ifdef __DEBUG__
		printk("at 0x%08x -> AT_PHENT = %d", sp, *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, sizeof(struct elf32_phdr), 1);
#ifdef __DEBUG__
		printk("\t\tAT_PHENT = %d\n", *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, AT_PHNUM, 1);
#ifdef __DEBUG__
		printk("at 0x%08x -> AT_PHNUM = %d", sp, *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, 0, 1);
		memcpy_w((void *)sp, &elf32_h->e_phnum, 1);
#ifdef __DEBUG__
		printk("\t\tAT_PHNUM = %d\n", *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, AT_PAGESZ, 1);
#ifdef __DEBUG__
		printk("at 0x%08x -> AT_PGSIZE = %d", sp, *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, PAGE_SIZE, 1);
#ifdef __DEBUG__
		printk("\t\tAT_PGSIZE = %d\n", *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, AT_BASE, 1);
#ifdef __DEBUG__
		printk("at 0x%08x -> AT_BASE = %d", sp, *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, at_base, 1);
#ifdef __DEBUG__
		printk("\t\tAT_BASE = 0x%08x\n", sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, AT_FLAGS, 1);
#ifdef __DEBUG__
		printk("at 0x%08x -> AT_FLAGS = %d", sp, *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, NULL, 1);
#ifdef __DEBUG__
		printk("\t\tAT_FLAGS = %d\n", *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, AT_ENTRY, 1);
#ifdef __DEBUG__
		printk("at 0x%08x -> AT_ENTRY = %d ", sp, *sp);
#endif /*__DEBUG__ */
		sp++;

		memcpy_l((void *)sp, &elf32_h->e_entry, 1);
#ifdef __DEBUG__
		printk("\t\tAT_ENTRY = 0x%08x\n", *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, AT_UID, 1);
#ifdef __DEBUG__
		printk("at 0x%08x -> AT_UID = %d", sp, *sp);
#endif /*__DEBUG__ */
		sp++;

		memcpy_l((void *)sp, &current->uid, 1);
#ifdef __DEBUG__
		printk("\t\tAT_UID = %d\n", *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, AT_EUID, 1);
#ifdef __DEBUG__
		printk("at 0x%08x -> AT_EUID = %d", sp, *sp);
#endif /*__DEBUG__ */
		sp++;

		memcpy_l((void *)sp, &current->euid, 1);
#ifdef __DEBUG__
		printk("\t\tAT_EUID = %d\n", *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, AT_GID, 1);
#ifdef __DEBUG__
		printk("at 0x%08x -> AT_GID = %d", sp, *sp);
#endif /*__DEBUG__ */
		sp++;

		memcpy_l((void *)sp, &current->gid, 1);
#ifdef __DEBUG__
		printk("\t\tAT_GID = %d\n", *sp);
#endif /*__DEBUG__ */
		sp++;

		memset_l((void *)sp, AT_EGID, 1);
#ifdef __DEBUG__
		printk("at 0x%08x -> AT_EGID = %d", sp, *sp);
#endif /*__DEBUG__ */
		sp++;

		memcpy_l((void *)sp, &current->egid, 1);
#ifdef __DEBUG__
		printk("\t\tAT_EGID = %d\n", *sp);
#endif /*__DEBUG__ */
		sp++;
	}

	memset_l((void *)sp, AT_NULL, 1);
#ifdef __DEBUG__
	printk("at 0x%08x -> AT_NULL = %d", sp, *sp);
#endif /*__DEBUG__ */
	sp++;

	*sp = NULL;
#ifdef __DEBUG__
	printk("\t\tAT_NULL = %d\n", *sp);
#endif /*__DEBUG__ */

#ifdef __DEBUG__
	for(n = 0; n < barg->argc; n++) {
		printk("at 0x%08x -> argv[%d] = '%s'\n", current->argv[n], n, current->argv[n]);
	}
	for(n = 0; n < barg->envc; n++) {
		printk("at 0x%08x -> envp[%d] = '%s'\n", current->envp[n], n, current->envp[n]);
	}
#endif /*__DEBUG__ */
}

static int elf_load_interpreter(struct inode *ii)
{
	int n, errno;
	struct buffer *buf;
	struct elf32_hdr *elf32_h;
	struct elf32_phdr *elf32_ph, *last_ptload;
	__blk_t block;
	unsigned int start, end, length;
	unsigned int prot;
	char *data;
	char type;

	if((block = bmap(ii, 0, FOR_READING)) < 0) {
		return block;
	}
	if(!(buf = bread(ii->dev, block, ii->sb->s_blocksize))) {
		return -EIO;
	}

	/*
	 * The contents of the buffer is copied and then freed immediately to
	 * make sure that it won't conflict while zeroing the BSS fractional
	 * page, in case that the same block is requested during the page fault.
	 */
	if(!(data = (void *)kmalloc())) {
		brelse(buf);
		return -ENOMEM;
	}
	memcpy_b(data, buf->data, ii->sb->s_blocksize);
	brelse(buf);

	elf32_h = (struct elf32_hdr *)data;
	if(check_elf(elf32_h)) {
		kfree((unsigned int)data);
		return -ELIBBAD;
	}

	last_ptload = NULL;
	for(n = 0; n < elf32_h->e_phnum; n++) {
		elf32_ph = (struct elf32_phdr *)(data + elf32_h->e_phoff + (sizeof(struct elf32_phdr) * n));
		if(elf32_ph->p_type == PT_LOAD) {
#ifdef __DEBUG__
			printk("p_offset = 0x%08x\n", elf32_ph->p_offset);
			printk("p_vaddr  = 0x%08x\n", elf32_ph->p_vaddr);
			printk("p_paddr  = 0x%08x\n", elf32_ph->p_paddr);
			printk("p_filesz = 0x%08x\n", elf32_ph->p_filesz);
			printk("p_memsz  = 0x%08x\n\n", elf32_ph->p_memsz);
#endif /*__DEBUG__ */
			start = (elf32_ph->p_vaddr & PAGE_MASK) + MMAP_START;
			length = (elf32_ph->p_vaddr & ~PAGE_MASK) + elf32_ph->p_filesz;
			type = P_DATA;
			prot = 0;
			if(elf32_ph->p_flags & PF_R) {
				prot = PROT_READ;
			}
			if(elf32_ph->p_flags & PF_W) {
				prot |= PROT_WRITE;
			}
			if(elf32_ph->p_flags & PF_X) {
				prot |= PROT_EXEC;
				type = P_TEXT;
			}
			errno = do_mmap(ii, start, length, prot, MAP_PRIVATE | MAP_FIXED, elf32_ph->p_offset & PAGE_MASK, type, O_RDONLY);
			if(errno < 0 && errno > -PAGE_SIZE) {
				kfree((unsigned int)data);
				send_sig(current, SIGSEGV);
				return -ENOEXEC;
			}
			last_ptload = elf32_ph;
		}
	}

	if(!last_ptload) {
		printk("WARNING: 'last_ptload' is NULL!\n");
	}
	elf32_ph = last_ptload;

	/* zero-fill the fractional page of the DATA section */
	end = PAGE_ALIGN(elf32_ph->p_vaddr + elf32_ph->p_filesz) + MMAP_START;
	start = (elf32_ph->p_vaddr + elf32_ph->p_filesz) + MMAP_START;
	length = end - start;

	/* this will generate a page fault which will load the page in */
	memset_b((void *)start, NULL, length);

	/* setup the BSS section */
	start = (elf32_ph->p_vaddr + elf32_ph->p_filesz) + MMAP_START;
	start = PAGE_ALIGN(start);
	end = (elf32_ph->p_vaddr + elf32_ph->p_memsz) + MMAP_START;
	end = PAGE_ALIGN(end);
	length = end - start;
	errno = do_mmap(NULL, start, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, 0, P_BSS, 0);
	if(errno < 0 && errno > -PAGE_SIZE) {
		kfree((unsigned int)data);
		send_sig(current, SIGSEGV);
		return -ENOEXEC;
	}
	kfree((unsigned int)data);
	return elf32_h->e_entry + MMAP_START;
}

int elf_load(struct inode *i, struct binargs *barg, struct sigcontext *sc, char *data)
{
	int n, errno;
	struct elf32_hdr *elf32_h;
	struct elf32_phdr *elf32_ph, *last_ptload;
	struct inode *ii;
	unsigned int start, end, length;
	unsigned int prot;
	char *interpreter;
	int at_base, phdr_addr;
	char type;
	unsigned int ae_ptr_len, ae_str_len;
	unsigned int sp, str;

	elf32_h = (struct elf32_hdr *)data;
	if(check_elf(elf32_h)) {
		if(current->pid == INIT) {
			PANIC("%s has an unrecognized binary format.\n", INIT_PROGRAM);
		}
		return -ENOEXEC;
	}

	if(elf32_h->e_phnum < 1) {
		printk("%s(): no program headers.");
		return -ENOEXEC;
	}

	/* check if an interpreter is required */
	interpreter = NULL;
	ii = NULL;
	phdr_addr = at_base = 0;
	for(n = 0; n < elf32_h->e_phnum; n++) {
		elf32_ph = (struct elf32_phdr *)(data + elf32_h->e_phoff + (sizeof(struct elf32_phdr) * n));
		if(elf32_ph->p_type == PT_INTERP) {
			at_base = MMAP_START;
			interpreter = data + elf32_ph->p_offset;
			if(namei(interpreter, &ii, NULL, FOLLOW_LINKS)) {
				printk("%s(): can't find interpreter '%s'.\n", __FUNCTION__, interpreter);
				send_sig(current, SIGSEGV);
				return -ELIBACC;
			}
#ifdef __DEBUG__
			printk("p_offset = 0x%08x\n", elf32_ph->p_offset);
			printk("p_vaddr  = 0x%08x\n", elf32_ph->p_vaddr);
			printk("p_paddr  = 0x%08x\n", elf32_ph->p_paddr);
			printk("p_filesz = 0x%08x\n", elf32_ph->p_filesz);
			printk("p_memsz  = 0x%08x\n", elf32_ph->p_memsz);
			printk("using interpreter '%s'\n", interpreter);
#endif /*__DEBUG__ */
		}
	}

	/*
	 * calculate the final size of 'ae_ptr_len' based on:
	 *  - argc = 4 bytes (unsigned int)
	 *  - barg.argc = (num. of pointers to strings + 1 NULL) x 4 bytes (unsigned int)
	 *  - barg.envc = (num. of pointers to strings + 1 NULL) x 4 bytes (unsigned int)
	 */
	ae_ptr_len = (1 + (barg->argc + 1) + (barg->envc + 1)) * sizeof(unsigned int);
	ae_str_len = barg->argv_len + barg->envp_len;
	if(ae_ptr_len + ae_str_len > (ARG_MAX * PAGE_SIZE)) {
		printk("WARNING: %s(): argument list (%d) exceeds ARG_MAX (%d)!\n", __FUNCTION__, ae_ptr_len + ae_str_len, ARG_MAX * PAGE_SIZE);
		return -E2BIG;
	}

#ifdef __DEBUG__
	printk("argc=%d (argv_len=%d) envc=%d (envp_len=%d)  ae_ptr_len=%d ae_str_len=%d\n", barg->argc, barg->argv_len, barg->envc, barg->envp_len, ae_ptr_len, ae_str_len);
#endif /*__DEBUG__ */


	/* point of no return */

	release_binary();
	current->rss = 0;

	current->entry_address = elf32_h->e_entry;
	if(interpreter) {
		errno = elf_load_interpreter(ii);
		if(errno < 0) {
			printk("%s(): unable to load the interpreter '%s'.\n", __FUNCTION__, interpreter);
			iput(ii);
			send_sig(current, SIGKILL);
			return errno;
		}
		current->entry_address = errno;
		iput(ii);
	}

	last_ptload = NULL;
	for(n = 0; n < elf32_h->e_phnum; n++) {
		elf32_ph = (struct elf32_phdr *)(data + elf32_h->e_phoff + (sizeof(struct elf32_phdr) * n));
		if(elf32_ph->p_type == PT_PHDR) {
			phdr_addr = elf32_ph->p_vaddr;
		}
		if(elf32_ph->p_type == PT_LOAD) {
			start = elf32_ph->p_vaddr & PAGE_MASK;
			length = (elf32_ph->p_vaddr & ~PAGE_MASK) + elf32_ph->p_filesz;
			type = P_DATA;
			prot = 0;
			if(elf32_ph->p_flags & PF_R) {
				prot = PROT_READ;
			}
			if(elf32_ph->p_flags & PF_W) {
				prot |= PROT_WRITE;
			}
			if(elf32_ph->p_flags & PF_X) {
				prot |= PROT_EXEC;
				type = P_TEXT;
			}
			errno = do_mmap(i, start, length, prot, MAP_PRIVATE | MAP_FIXED, elf32_ph->p_offset & PAGE_MASK, type, O_RDONLY);
			if(errno < 0 && errno > -PAGE_SIZE) {
				send_sig(current, SIGSEGV);
				return -ENOEXEC;
			}
			last_ptload = elf32_ph;
		}
	}

	elf32_ph = last_ptload;

	/* zero-fill the fractional page of the DATA section */
	end = PAGE_ALIGN(elf32_ph->p_vaddr + elf32_ph->p_filesz);
	start = elf32_ph->p_vaddr + elf32_ph->p_filesz;
	length = end - start;

	/* this will generate a page fault which will load the page in */
	memset_b((void *)start, NULL, length);

	/* setup the BSS section */
	start = elf32_ph->p_vaddr + elf32_ph->p_filesz;
	start = PAGE_ALIGN(start);
	end = elf32_ph->p_vaddr + elf32_ph->p_memsz;
	end = PAGE_ALIGN(end);
	length = end - start;
	errno = do_mmap(NULL, start, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, 0, P_BSS, 0);
	if(errno < 0 && errno > -PAGE_SIZE) {
		send_sig(current, SIGSEGV);
		return -ENOEXEC;
	}
	current->brk_lower = start;

	/* setup the HEAP section */
	start = elf32_ph->p_vaddr + elf32_ph->p_memsz;
	start = PAGE_ALIGN(start);
	length = PAGE_SIZE;
	errno = do_mmap(NULL, start, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, 0, P_HEAP, 0);
	if(errno < 0 && errno > -PAGE_SIZE) {
		send_sig(current, SIGSEGV);
		return -ENOEXEC;
	}
	current->brk = start;

	/* setup the STACK section */
	sp = KERNEL_BASE_ADDR - 4;	/* formerly 0xBFFFFFFC */
	sp -= ae_str_len;
	str = sp;	/* this is the address of the first string (argv[0]) */
	sp &= ~3;
	sp -= at_base ? (AT_ITEMS * 2) * sizeof(unsigned int) : 2 * sizeof(unsigned int);
	sp -= ae_ptr_len;
	length = KERNEL_BASE_ADDR - (sp & PAGE_MASK);
	errno = do_mmap(NULL, sp & PAGE_MASK, length, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_FIXED, 0, P_STACK, 0);
	if(errno < 0 && errno > -PAGE_SIZE) {
		send_sig(current, SIGSEGV);
		return -ENOEXEC;
	}

	elf_create_stack(barg, (unsigned int *)sp, str, at_base, elf32_h, phdr_addr);

	/* set %esp to point to 'argc' */
	sc->oldesp = sp;
	sc->eflags = 0x202;	/* FIXME: linux 2.2 = 0x292 */
	sc->eip = current->entry_address;
	sc->err = 0;
	sc->eax = 0;
	sc->ecx = 0;
	sc->edx = 0;
	sc->ebx = 0;
	sc->ebp = 0;
	sc->esi = 0;
	sc->edi = 0;
	return 0;
}
