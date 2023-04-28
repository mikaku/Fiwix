/*
 * fiwix/kernel/kexec.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/system.h>
#include <fiwix/config.h>
#include <fiwix/kexec.h>
#include <fiwix/segments.h>
#include <fiwix/mm.h>
#include <fiwix/sched.h>
#include <fiwix/ramdisk.h>
#include <fiwix/multiboot1.h>
#include <fiwix/bios.h>
#include <fiwix/i386elf.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#ifdef CONFIG_KEXEC

int kexec_proto;
int kexec_size;
char kexec_cmdline[NAME_MAX + 1];

static void _memcpy_b(void *dest, const void *src, unsigned int count)
{
	unsigned char *d, *s;

	s = (unsigned char *)src;
	d = (unsigned char *)dest;
	while(count--) {
		*d = *s;
		d++;
		s++;
	}
}

static void _memset_b(void *dest, unsigned char value, unsigned int count)
{
	unsigned char *d;

	d = (unsigned char *)dest;
	while(count--) {
		*d = 0;
		d++;
	}
}

static void multiboot1_trampoline(unsigned int ramdisk_addr, unsigned int kernel_addr, int size, struct multiboot_info *info)
{
	struct elf32_phdr *elf32_ph;
	struct elf32_hdr *elf32_h;
	unsigned int start, length, offset;
	unsigned int entry_addr;

	struct gate_desc idt[64];
	struct desc_r idtr = {
		sizeof(idt) - 1,
		(unsigned int)&idt
	};

	struct seg_desc gdt[3];
	struct desc_r gdtr = {
		sizeof(gdt) - 1,
		(unsigned int)&gdt
	};

	int n, cr0;


	CLI();

	/* invalidate IDT */
	_memset_b(&idt, 0, sizeof(idt));
	__asm__ __volatile__ (
		"lidt %0\n\t"
		: /* no output */
		: "m"(idtr)
	);

	/* configure a new flat GDT */
	gdt[0].sd_lolimit = 0;
	gdt[0].sd_lobase = 0;
	gdt[0].sd_loflags = 0;
	gdt[0].sd_hilimit = 0;
	gdt[0].sd_hiflags = 0;
	gdt[0].sd_hibase = 0;

	gdt[1].sd_lolimit = 0xFFFF;
	gdt[1].sd_lobase = 0x00000000 & 0xFFFFFF;
	gdt[1].sd_loflags = SD_CODE | SD_CD | SD_DPL0 | SD_PRESENT;
	gdt[1].sd_hilimit = (0xFFFFFFFF >> 16) & 0x0F;
	gdt[1].sd_hiflags = SD_OPSIZE32 | SD_PAGE4KB;
	gdt[1].sd_hibase = (0x00000000 >> 24) & 0xFF;

	gdt[2].sd_lolimit = 0xFFFF;
	gdt[2].sd_lobase = 0x00000000 & 0xFFFFFF;
	gdt[2].sd_loflags = SD_DATA | SD_CD | SD_DPL0 | SD_PRESENT;
	gdt[2].sd_hilimit = (0xFFFFFFFF >> 16) & 0x0F;
	gdt[2].sd_hiflags = SD_OPSIZE32 | SD_PAGE4KB;
	gdt[2].sd_hibase = (0x00000000 >> 24) & 0xFF;
	__asm__ __volatile__ (
		"lgdt %0\n\t"
		: /* no output */
		: "m"(gdtr)
	);

	/* disable paging and others */
	cr0 = 0x11;	/* preserve ET & PE */
	__asm__ __volatile__(
		"mov	%0, %%cr0"
		: /* no output */
		: "r"(cr0)
	);

	/* flush TLB (needed?) */
	__asm__ __volatile__(
		"xorl	%%eax, %%eax\n\t"
		"movl	%%eax, %%cr3\n\t"
		: /* no output */
		: /* no input */
		: "%eax"	/* clobbered registers */
	);

	/*
	 * Clear memory. This is intended to avoid unexpected results if the
	 * new kernel guesses its uninitialized variables are zeroed.
	 */
	_memset_b(0x0, 0, KEXEC_BOOT_ADDR);
	_memset_b((void *)0x100000, 0, ramdisk_addr - 0x100000);

	/* install the kernel previously stored in RAMdisk by the user */
	elf32_h = (struct elf32_hdr *)ramdisk_addr;
	entry_addr = elf32_h->e_entry;

	for(n = 0; n < elf32_h->e_phnum; n++) {
		elf32_ph = (struct elf32_phdr *)(ramdisk_addr + elf32_h->e_phoff + (sizeof(struct elf32_phdr) * n));
		if(elf32_ph->p_type == PT_LOAD) {
			start = elf32_ph->p_paddr;
			length = elf32_ph->p_filesz;
			offset = elf32_ph->p_offset;
			_memcpy_b((void *)start, (unsigned char *)(ramdisk_addr + offset), length);
		}
	}

	/* flush TLB (needed?) */
	__asm__ __volatile__(
		"xorl	%%eax, %%eax\n\t"
		"movl	%%eax, %%cr3\n\t"
		: /* no output */
		: /* no input */
		: "%eax"	/* clobbered registers */
	);

	/* load all the segment registers with the kernel data segment value */
	__asm__ __volatile__(
		"movw	$0x10, %%ax\n\t"
		"movw	%%ax, %%ds\n\t"
		"movw	%%ax, %%es\n\t"
		"movw	%%ax, %%fs\n\t"
		"movw	%%ax, %%gs\n\t"
		"movw	%%ax, %%ss\n\t"
		: /* no output */
		: /* no input */
		: "%eax"	/* clobbered registers */
	);

	/* Multiboot 1 */
	__asm__ __volatile__(
		"movl	%0, %%eax\n\t"
		"movl	%1, %%ebx\n\t"
		: /* no output */
		: "eax"(MULTIBOOT_BOOTLOADER_MAGIC), "ebx"((unsigned int)info)
	);

	/*
	 * This jumps to the kernel entry address.
	 *
	 * Formerly: ljmp $0x08, $entry_addr
	 */
	__asm__ __volatile__(
		"pushw	$0x08\n\t"
		"pushl	%0\n\t"
		"ljmp	*(%%esp)\n\t"
		: /* no output */
		: "c"(entry_addr)
	);

	/* not reached */
}

void kexec_multiboot1(void)
{
	unsigned int *esp, ramdisk_addr;
	struct proc *idle, *prev;
	struct multiboot_info *info;
	struct multiboot_mmap_entry *map, *map_orig;
	struct elf32_hdr *elf32_h;
	char *cmdline;
	char *boot_loader_name;
	int n, nmaps;

	CLI();

	ramdisk_addr = (unsigned int)ramdisk_table[ramdisk_minors - 1].addr;
	elf32_h = (struct elf32_hdr *)ramdisk_addr;
	if(check_elf(elf32_h)) {
		printk("WARNING: %s(): unrecognized i386 binary ELF.\n", __FUNCTION__);
		return;
	}

	memcpy_b((void *)KEXEC_BOOT_ADDR, multiboot1_trampoline, PAGE_SIZE);

	/* the IDLE process will do the job */
	idle = &proc_table[IDLE];
	idle->tss.eip = (unsigned int)KEXEC_BOOT_ADDR;

	/* stack starts at the end of the page */
	esp = (unsigned int *)(KEXEC_BOOT_ADDR + PAGE_SIZE - 4);

	/* space reserved for the cmdline string (256 bytes) */
	esp -= 256 / sizeof(unsigned int);
	cmdline = (char *)esp;
	sprintk(cmdline, "%s %s", UTS_SYSNAME, kexec_cmdline);

	/* space reserved for the boot_loader_name string (16 bytes) */
	esp -= 16 / sizeof(unsigned int);
	boot_loader_name = (char *)esp;
	sprintk(boot_loader_name, "Fiwix kexec");

	/* space reserved for the memory map structure */
	nmaps = 0;
	while(bios_mem_map[nmaps].to) {
		nmaps++;
	}
	esp -= sizeof(struct multiboot_mmap_entry) * nmaps;
	map_orig = map = (struct multiboot_mmap_entry *)esp;
	/* setup the memory map */
	for(n = 0; n < nmaps; n++) {
		map->size = sizeof(struct multiboot_mmap_entry) - sizeof(map->size);
		map->addr = bios_mem_map[n].from;
		map->len = (bios_mem_map[n].to + 1) - bios_mem_map[n].from;
		map->type = bios_mem_map[n].type;
		map++;
	}

	/* space reserved for the multiboot_info structure */
	esp -= sizeof(struct multiboot_info) / sizeof(unsigned int);
	memset_b(esp, 0, sizeof(struct multiboot_info));
	info = (struct multiboot_info *)esp;

	/* setup Multiboot structure */
	info->flags = MULTIBOOT_INFO_MEMORY;
	info->mem_lower = kparm_memsize;
	info->mem_upper = kparm_extmemsize;
	info->flags |= MULTIBOOT_INFO_CMDLINE;
	info->cmdline = (unsigned int)cmdline;
	info->flags |= MULTIBOOT_INFO_BOOT_LOADER_NAME;
	info->boot_loader_name = (unsigned int)boot_loader_name;
	info->flags |= MULTIBOOT_INFO_MEM_MAP;
	info->mmap_length = sizeof(struct multiboot_mmap_entry) * nmaps;
	info->mmap_addr = (unsigned int)map_orig;
	esp--;

	/* now put the four parameters into the stack */
	*esp = (unsigned int)info;
	esp--;
	*esp = kexec_size * 1024;
	esp--;
	*esp = KERNEL_ADDR;
	esp--;
	*esp = V2P(ramdisk_addr);
	esp--;
	idle->tss.esp = (unsigned int)esp;

	printk("%s(): jumping to multiboot1_trampoline() ...\n", __FUNCTION__);
	prev = current;
	set_tss(idle);
	do_switch(&prev->tss.esp, &prev->tss.eip, idle->tss.esp, idle->tss.eip, idle->tss.cr3, TSS);
}

#endif /* CONFIG_KEXEC */
