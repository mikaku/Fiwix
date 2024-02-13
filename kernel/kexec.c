/*
 * fiwix/kernel/kexec.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Copyright (C) 2019-2024 mintsuki and contributors, Limine project.
 * Copyright 2023-2024, Richard R. Masters.
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
#include <fiwix/linuxboot.h>
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
		: "eax"	/* clobbered registers */
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
		: "eax"	/* clobbered registers */
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
		: "eax"	/* clobbered registers */
	);

	/* Multiboot 1 */
#ifdef __TINYC__
	unsigned int multiboot_magic = MULTIBOOT_BOOTLOADER_MAGIC;
	__asm__ __volatile__(
		"movl	%0, %%eax\n\t"
		"movl	%1, %%ebx\n\t"
		: /* no output */
		: "r"(multiboot_magic), "r"((unsigned int)info)
	);
#else
	__asm__ __volatile__(
		"movl	%0, %%eax\n\t"
		"movl	%1, %%ebx\n\t"
		: /* no output */
		: "eax"(MULTIBOOT_BOOTLOADER_MAGIC), "ebx"((unsigned int)info)
	);
#endif

	/*
	 * This jumps to the kernel entry address.
	 *
	 * i.e.: ljmp $0x08, $entry_addr
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
	while(orig_bios_mem_map[nmaps].to) {
		nmaps++;
	}
	esp -= sizeof(struct multiboot_mmap_entry) * nmaps;
	map_orig = map = (struct multiboot_mmap_entry *)esp;
	/* setup the memory map */
	for(n = 0; n < nmaps; n++) {
		map->size = sizeof(struct multiboot_mmap_entry) - sizeof(map->size);
		map->addr = orig_bios_mem_map[n].from_hi;
		map->addr = map->addr << 32 | orig_bios_mem_map[n].from;
		map->len = orig_bios_mem_map[n].to_hi;
		map->len = map->len << 32 | orig_bios_mem_map[n].to;
		map->len -= map->addr - 1;
		map->type = orig_bios_mem_map[n].type;
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
	/* not reached */
	return;
}

static void linux_trampoline(char *kernel_src_addr, unsigned int kernel_size,
		char *initrd_src_addr, unsigned int initrd_size,
		char *initrd_dst_addr,
		struct boot_params *boot_params)
{
	struct gate_desc idt[64];
	struct desc_r idtr = {
		sizeof(idt) - 1,
		(unsigned int)&idt
	};

	struct seg_desc gdt[4];
	struct desc_r gdtr = {
		sizeof(gdt) - 1,
		(unsigned int)&gdt
	};

	int cr0;

	CLI();

	/* invalidate IDT */
	char *dst, *src;
	unsigned int len;
	for (dst = (char *)&idt, len = sizeof(idt); len; len--) {
		*dst++ = 0;
	}

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

	gdt[1].sd_lolimit = 0;
	gdt[1].sd_lobase = 0;
	gdt[1].sd_loflags = 0;
	gdt[1].sd_hilimit = 0;
	gdt[1].sd_hiflags = 0;
	gdt[1].sd_hibase = 0;

	gdt[2].sd_lolimit = 0xFFFF;
	gdt[2].sd_lobase = 0x00000000 & 0xFFFFFF;
	gdt[2].sd_loflags = SD_CODE | SD_CD | SD_DPL0 | SD_PRESENT;
	gdt[2].sd_hilimit = (0xFFFFFFFF >> 16) & 0x0F;
	gdt[2].sd_hiflags = SD_OPSIZE32 | SD_PAGE4KB;
	gdt[2].sd_hibase = (0x00000000 >> 24) & 0xFF;

	gdt[3].sd_lolimit = 0xFFFF;
	gdt[3].sd_lobase = 0x00000000 & 0xFFFFFF;
	gdt[3].sd_loflags = SD_DATA | SD_CD | SD_DPL0 | SD_PRESENT;
	gdt[3].sd_hilimit = (0xFFFFFFFF >> 16) & 0x0F;
	gdt[3].sd_hiflags = SD_OPSIZE32 | SD_PAGE4KB;
	gdt[3].sd_hibase = (0x00000000 >> 24) & 0xFF;
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
		: "eax"	/* clobbered registers */
	);

	/* Note that below we avoid using _memset_b and _memcpy_b because we cannot
	 * make function calls because this trampoline has been relocated.
	 */

	/*
	 * Clear memory. This is intended to avoid unexpected results if the
	 * new kernel guesses its uninitialized variables are zeroed.
	 */
	for (dst = (char *)0, len = KEXEC_BOOT_ADDR; len; len--) {
		*dst++ = 0;
	}

	for (dst = (char *)0x100000, len = (unsigned int)kernel_src_addr - 0x100000; len; len--) {
		*dst++ = 0;
	}

	/* install the kernel previously stored in RAMdisk by the user */
	for (dst = (char *)0x100000, src = kernel_src_addr, len = kernel_size; len; len--) {
		*dst++ = *src++;
	}

	/* install the ramdisk previously stored in RAMdisk by the user */
	for (dst = (char *)initrd_dst_addr, src = initrd_src_addr, len = initrd_size; len; len--) {
		*dst++ = *src++;
	}

	/* flush TLB (needed?) */
	__asm__ __volatile__(
		"xorl	%%eax, %%eax\n\t"
		"movl	%%eax, %%cr3\n\t"
		: /* no output */
		: /* no input */
		: "eax"	/* clobbered registers */
	);

	/* load all the segment registers with the kernel data segment value */
	__asm__ __volatile__(
		"movw	$0x18, %%ax\n\t"
		"movw	%%ax, %%ds\n\t"
		"movw	%%ax, %%es\n\t"
		"movw	%%ax, %%fs\n\t"
		"movw	%%ax, %%gs\n\t"
		"movw	%%ax, %%ss\n\t"
		: /* no output */
		: /* no input */
		: "eax"	/* clobbered registers */
	);

	/* Linux boot */
#ifdef __TINYC__
	__asm__ __volatile__(
		"movl	%0, %%eax\n\t"
		"movl	%%eax, %%esi\n\t"
		: /* no output */
		: "r"((unsigned int)boot_params)
	);
#else
	__asm__ __volatile__(
		"movl	%0, %%eax\n\t"
		"movl	%%eax, %%esi\n\t"
		: /* no output */
		: "eax"((unsigned int)boot_params)
	);
#endif

	/*
	 * This jumps to the kernel entry address.
	 *
	 */
	__asm__ __volatile__(
		"pushw	$0x10\n\t"
		"pushl	%0\n\t"
		"ljmp	*(%%esp)\n\t"
		: /* no output */
		: "c"(KERNEL_ADDR)
	);

	/* not reached */
}


void kexec_linux(void)
{
	struct proc *idle, *prev;
	unsigned int *esp;
	char *kernel_src_addr, *initrd_src_addr;
	__u32 kernel_size, initrd_size;
	struct boot_params *boot_params;
	char *cmdline;

	kernel_size = *((__u32 *)ramdisk_table[ramdisk_minors - 1].addr);
	printk("kexec_linux: kernel file size: %u\n", kernel_size);
	initrd_size = *((__u32 *) (ramdisk_table[ramdisk_minors - 1].addr + sizeof(__u32)));

	/* space reserved for the memory map structure */
	kernel_src_addr = ramdisk_table[ramdisk_minors - 1].addr + (sizeof(__u32) * 2);

	__u32 signature;
	memcpy_b(&signature, kernel_src_addr + 0x202, sizeof(__u32));

	/* validate signature */
	if (signature != 0x53726448) {
		printk("kexec_linux: Invalid kernel signature\n");
		return;
	} else {
		printk("kexec_linux: Valid kernel signature\n");
	}

	__size_t setup_code_size = 0;
	memcpy_b(&setup_code_size, kernel_src_addr + 0x1f1, 1);
	if (setup_code_size == 0) {
		setup_code_size = 4;
	}
	setup_code_size *= 512;

	__size_t real_mode_code_size = 512 + setup_code_size;

	memcpy_b((void *)KEXEC_BOOT_ADDR, linux_trampoline, PAGE_SIZE);

	/* the IDLE process will do the job */
	idle = &proc_table[IDLE];
	idle->tss.eip = (unsigned int)KEXEC_BOOT_ADDR;

	/* stack starts at the end of the page */
	esp = (unsigned int *)(KEXEC_BOOT_ADDR + (PAGE_SIZE * 2) - 4);

	/* space reserved for the cmdline string (256 bytes) */
	esp -= 256 / sizeof(unsigned int);
	cmdline = (char *)esp;
	strcpy(cmdline, kexec_cmdline);

	/* space reserved for the boot_params structure */
	esp -= sizeof(struct boot_params) / sizeof(unsigned int);
	memset_b(esp, 0, sizeof(struct boot_params));
	boot_params = (struct boot_params *)esp;

	struct setup_header *setup_header = &boot_params->hdr;

	__size_t setup_header_end = ({
		__u8 x;
		memcpy_b(&x, kernel_src_addr + 0x201, 1);
		0x202 + x;
	});

	memcpy_b(setup_header, kernel_src_addr + 0x1f1, setup_header_end - 0x1f1);

	printk("kexec_linux: Boot protocol: %u.%u\n",
		setup_header->version >> 8, setup_header->version & 0xff);

	if (setup_header->version < 0x203) {
		printk("kexec_linux: Protocols < 2.03 are not supported");
		return;
	}

	setup_header->cmdline_addr = (__u32)(unsigned int *)cmdline;

	/* video_mode. 0xffff means "normal" */
	setup_header->video_mode = 0xffff;

	/* 0xff means this loader does not have an assigned id */
	setup_header->loader_type = 0xff;

	if (!(setup_header->load_flags & (1 << 0))) {
		printk("kexec_linux: Kernels that load at 0x10000 are not supported");
		return;
	}

	setup_header->load_flags &= ~(1 << 5);     /* print early messages */

	/* Modules */

	__u32 modules_mem_base = setup_header->initramfs_addr_max;
	if (modules_mem_base == 0) {
		modules_mem_base = 0x38000000;
	}

	initrd_src_addr = kernel_src_addr + kernel_size;

	modules_mem_base -= initrd_size;

	if (initrd_size) {
		setup_header->initramfs_addr = (__u32)modules_mem_base;
	} else {
		setup_header->initramfs_addr = 0;
	}
	setup_header->initramfs_size  = (__u32)initrd_size;

	struct screen_info *screen_info = &boot_params->screen_info;

	screen_info->mode = 3;
	screen_info->ega_bx = 3;
	screen_info->lines = 25;
	screen_info->cols = 80;
	screen_info->points = 16;

	screen_info->is_vga = IS_VGA_VGA_COLOR;

	/* e820 bios memory entries */

	struct bios_mem_entry *bios_mem_table = boot_params->bios_mem_table;

	__size_t i, j;
	for (i = 0, j = 0; i < NR_BIOS_MM_ENT; i++) {
                if(!orig_bios_mem_map[i].type || orig_bios_mem_map[i].type > 0x1000) {
			continue;
		}
		bios_mem_table[j].addr = orig_bios_mem_map[i].from_hi;
		bios_mem_table[j].addr = (bios_mem_table[j].addr << 32) | orig_bios_mem_map[i].from;
		bios_mem_table[j].size = orig_bios_mem_map[i].to_hi;
		bios_mem_table[j].size = (bios_mem_table[j].size << 32) | orig_bios_mem_map[i].to;
		bios_mem_table[j].size -= bios_mem_table[j].addr - 1;
		bios_mem_table[j].type = orig_bios_mem_map[i].type;
		j++;
		boot_params->num_bios_mem_entries = j;
	}

	/* now put the six parameters into the stack */
	esp--;
	*esp = (unsigned int)boot_params;
	esp--;
	*esp = modules_mem_base;
	esp--;
	*esp = initrd_size;
	esp--;
	*esp = (unsigned int)V2P(initrd_src_addr);
	esp--;
	*esp = (kernel_size - real_mode_code_size);
	esp--;
	*esp = (unsigned int)V2P(kernel_src_addr + real_mode_code_size);
	esp--;
	idle->tss.esp = (unsigned int)esp;

	printk("kexec_linux: boot_params: %x\n", boot_params);
	printk("kexec_linux: modules_mem_base: %x\n", modules_mem_base);
	printk("kexec_linux: initrd_size: %u\n", initrd_size);
	printk("kexec_linux: initrd_src_addr: %x\n", initrd_src_addr);
	printk("kexec_linux: initrd_src_addr: %x\n", V2P(initrd_src_addr));
	printk("kexec_linux: kernel_size: %u\n", kernel_size - real_mode_code_size);
	printk("kexec_linux: kernel_src_addr: %x\n", kernel_src_addr + real_mode_code_size);
	printk("kexec_linux: kernel_src_addr: %x\n", V2P(kernel_src_addr + real_mode_code_size));

	printk("kexec_linux: jumping to linux_trampoline() ...\n");
	prev = current;
	set_tss(idle);
	do_switch(&prev->tss.esp, &prev->tss.eip, idle->tss.esp, idle->tss.eip, idle->tss.cr3, TSS);
	/* not reached */
	return;
}

#endif /* CONFIG_KEXEC */
