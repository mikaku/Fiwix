/*
 * fiwix/kernel/boot.S
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/const.h>

#define ASM_FILE	1	/* GRUB stuff */
#include <fiwix/multiboot1.h>

/* The flags for the Multiboot header. */
#ifdef __ELF__
# define AOUT_KLUDGE 0
#else
# define AOUT_KLUDGE MULTIBOOT_AOUT_KLUDGE
#endif
#define MULTIBOOT_HEADER_FLAGS	MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE | AOUT_KLUDGE

#define CR0_MP	0x00000002	/* CR0 bit-01 MP (Monitor Coprocessor) */
#define CR0_NE	0x00000020	/* CR0 bit-05 NE (Numeric Error) */
#define CR0_WP	0x00010000	/* CR0 bit-16 WP (Write Protect) */
#define CR0_AM	0x00040000	/* CR0 bit-18 AM (Alignment Mask) */
#define CR0_PG	0x80000000	/* CR0 bit-31 PG (Paging) */

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


.text

.align 4
multiboot_header:			/* multiboot header */
	.long	MULTIBOOT_HEADER_MAGIC	/* magic */
	.long	MULTIBOOT_HEADER_FLAGS	/* flags */
	/* checksum */
	.long	-(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

	.long	0			/* header_addr */
	.long	0			/* load_addr */
	.long	0			/* load_end_addr */
	.long	0			/* bss_end_addr */
	.long	0			/* entry_addr */

	.long	0			/* mode_type */
	.long	1024			/* width */
	.long	768			/* height */
	.long	32			/* depth */

/*
 * We use the CX register in order to keep intact the values in AX and BX
 * registers, since they are holding the Multiboot values 'magic' and 'info'
 * respectively.
 */
.globl start; start:
	lgdt	tmp_gdtr		/* load GDTR with the temporary GDT */
	movw	$KERNEL_DS, %cx
	movw	%cx, %ds
	movw	%cx, %es
	movw	%cx, %fs
	movw	%cx, %gs
	movw	%cx, %ss
	ljmp	$KERNEL_CS, $1f
1:


/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING !!!
 * ---------------------------------------------------------------------------
 * The minimal page directory of 4MB only works if the in-memory size of the
 * kernel is lesser than 3MB. If you need more space go to the setup_minmem()
 * function and set the 'mb4' variable accordingly.
 *
 * In order to know the current size of the Fiwix kernel, just follow this:
 *
 * # readelf -l fiwix
 *   Elf file type is EXEC (Executable file)
 *   Entry point 0xc0100020
 *   There are 2 program headers, starting at offset 52
 *
 *   Program Headers:
 *     Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
 *     LOAD           0x000074 0x00100000 0x00100000 0x00020 0x00020 R   0x4
 *     LOAD           0x0000a0 0xc0100020 0x00100020 0x33f8c 0x859a0 RWE 0x20
 *                                      check this value --> ^^^^^^^
 */
	movl	$0xC0010000, %esp	/* default stack address */
	pushl	$0			/* reset EFLAGS */
	popf

	pushl	%eax			/* save Multiboot magic value */
	call	setup_minmem		/* setup a minimal page directory */
	movl	%eax, %cr3

	movl	%cr0, %eax
	andl	$0x00000011, %eax	/* disable all, preserve ET & PE (GRUB) */
	orl	$CR0_PG, %eax		/* enable PG (Paging) */
	orl	$CR0_AM, %eax		/* enable AM (Alignment Mask) */
	orl	$CR0_WP, %eax		/* enable WP (Write Protect) */
	orl	$CR0_NE, %eax		/* enable NE (Numeric Error) */
	orl	$CR0_MP, %eax		/* enable MP (Monitor Coprocessor) */
	movl	%eax, %cr0

	call	bss_init		/* initialize BSS segment */
	call	gdt_init		/* setup and load the definitive GDT */

	pushl	%ebx			/* save Multiboot info structure */
	call	get_last_boot_addr
	add	$4, %esp
	popl	%ecx			/* restore Multiboot magic value */
	andl	$0xFFFFF000, %eax	/* page aligned */
	addl	$0x3000, %eax		/* 2 whole pages for kernel stack */
	subl	$4, %eax
	movl	%eax, %esp		/* set kernel stack */

	pushl	%esp			/* save kernel stack address */
	pushl	%ebx			/* save Multiboot info structure */
	pushl	%ecx			/* save Multiboot magic value */
	call	start_kernel
	jmp	cpu_idle

.align 4
.org 0x1000
.globl  _fdc_transfer_area
_fdc_transfer_area:	.fill 512*2*18,1,0
