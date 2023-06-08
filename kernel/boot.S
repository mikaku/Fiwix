/*
 * fiwix/kernel/boot.S
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#define ASM_FILE	1

#include <fiwix/segments.h>
#include <fiwix/multiboot1.h>

#define MULTIBOOT_HEADER_FLAGS	MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO

/* flags for CR0 (control register) */
#define CR0_MP	0x00000002	/* bit 01 -> enable monitor coprocessor */
#define CR0_NE	0x00000020	/* bit 05 -> enable native x87 FPU mode */
#define CR0_WP	0x00010000	/* bit 16 -> enable write protect (for CoW) */
#define CR0_AM	0x00040000	/* bit 18 -> enable alignment checking */
#define CR0_PG	0x80000000	/* bit 31 -> enable paging */

.section .setup, "a"		/* "a" attribute means Allocatable section */

.align 4
tmp_gdtr:
	.word ((3 * 8) - 1)
	.long tmp_gdt

.align 4
tmp_gdt:
	/* NULL DESCRIPTOR */
	.word	0x0000
	.word	0x0000
	.word	0x0000
	.word	0x0000

	/* KERNEL CODE */
	.word	0xFFFF		/* segment limit 15-00 */
	.word	0x0000		/* base address 15-00 */
	.byte	0x00		/* base address 23-16 */
	.byte	0x9A		/* P=1 DPL=00 S=1 TYPE=1010 (exec/read) */
	.byte	0xCF		/* G=1 DB=1 0=0 AVL=0 SEGLIM=1111 */
	.byte	0x40		/* base address 31-24 */

	/* KERNEL DATA */
	.word	0xFFFF		/* segment limit 15-00 */
	.word	0x0000		/* base address 15-00 */
	.byte	0x00		/* base address 23-16 */
	.byte	0x92		/* P=1 DPL=00 S=1 TYPE=0010 (read/write) */
	.byte	0xCF		/* G=1 DB=1 0=0 AVL=0 SEGLIM=1111 */
	.byte	0x40		/* base address 31-24 */


.align 4
multiboot_header:			/* multiboot header */
	.long	MULTIBOOT_HEADER_MAGIC	/* magic */
	.long	MULTIBOOT_HEADER_FLAGS	/* flags */
	/* checksum */
	.long	-(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

	/* no used */
	.long	0			/* header_addr */
	.long	0			/* load_addr */
	.long	0			/* load_end_addr */
	.long	0			/* bss_end_addr */
	.long	0			/* entry_addr */

	/* valid only with GRUB2 */
	.long	0			/* mode_type */
	.long	0			/* width */
	.long	0			/* height */
	.long	0			/* depth */

/*
 * We use the CX register in order to keep intact the values in AX and BX
 * registers, since they are holding the Multiboot values 'magic' and 'info'
 * respectively.
 */
.align 4
.globl start; start:
	cli
	lgdt	tmp_gdtr		/* load GDTR with the temporary GDT */
	movw	$KERNEL_DS, %cx
	movw	%cx, %ds
	movw	%cx, %es
	movw	%cx, %fs
	movw	%cx, %gs
	movw	%cx, %ss
	ljmp	$KERNEL_CS, $setup_kernel

/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING !!!
 * ---------------------------------------------------------------------------
 * The minimal page directory of 4MB only works if the in-memory size of the
 * kernel is smaller than 3MB (which I presume will be its size for a very long
 * time). If you need more space go to the setup_minmem() function and set the
 * 'mb4' variable accordingly.
 *
 * In order to know the current size of the Fiwix kernel, just follow this:
 *
 * $ readelf -l fiwix
 *   Elf file type is EXEC (Executable file)
 *   Entry point 0x100050
 *   There are 2 program headers, starting at offset 52
 *
 *   Program Headers:
 *     Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
 *     LOAD           0x000074 0x00100000 0x00100000 0x0006d 0x0006d R   0x4
 *     LOAD           0x0000f0 0xc0100070 0x00100070 0x40540 0x5a6b0 RWE 0x20
 *                                      check this value --> ^^^^^^^
 *   ...
 */
.text

.align 4
.globl setup_kernel; setup_kernel:
	movl	$0xC0010000, %esp	/* default stack address */
	pushl	$0			/* reset EFLAGS */
	popf

	pushl	%eax			/* save Multiboot magic value */
	call	setup_minmem		/* setup a minimal page directory */
	movl	%eax, %cr3

	movl	%cr0, %eax
	andl	$0x00000011, %eax	/* disable all, preserve ET & PE (GRUB) */
	orl	$CR0_PG, %eax		/* enable PG */
	orl	$CR0_AM, %eax		/* enable AM */
	orl	$CR0_WP, %eax		/* enable WP */
	orl	$CR0_NE, %eax		/* enable NE */
	orl	$CR0_MP, %eax		/* enable MP */
	movl	%eax, %cr0

	call	bss_init		/* initialize BSS segment */
	call	gdt_init		/* setup and load the definitive GDT */

	popl	%ecx			/* restore Multiboot magic value */
	pushl	%ebx			/* save Multiboot info structure */
	pushl	%ecx			/* save Multiboot magic value */
	call	get_last_boot_addr
	popl	%ecx			/* restore Multiboot magic value */
	popl	%ebx			/* restore Multiboot info structure */
	andl	$0xFFFFF000, %eax	/* page aligned */

	pushl	%eax			/* save last boot address */
	pushl	%ebx			/* save Multiboot info structure */
	pushl	%ecx			/* save Multiboot magic value */
	call	start_kernel

	/* not reached */
	jmp	cpu_idle

