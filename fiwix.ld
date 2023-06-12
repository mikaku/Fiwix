/*
 * Linker script for the Fiwix kernel.
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/linker.h>

OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH("i386")
ENTRY(start)			/* entry point */
kstack_size = KERNEL_STACK;	/* size of the kernel stack */
vaddr = PAGE_OFFSET;		/* virtual start address */
paddr = KERNEL_ADDR;		/* physical address at 1MB */

/* define output sections */
SECTIONS
{
	. = paddr;

	/* kernel setup code */
	.setup ALIGN(4096) :
	{
		*(.setup)
	}

	. += vaddr;

	/* kernel code */
	.text : AT(ADDR(.text) - vaddr)
	{
		*(.text)
	}
	_etext = .;

	/* initialized data */
	.data ALIGN(4096) : AT(ADDR(.data) - vaddr)
	{
		*(.data)
		*(.rodata*)
	}
	_edata = .;

	/* uninitialized data */
	.bss ALIGN(4096) : AT(ADDR(.bss) - vaddr)
	{
		*(COMMON*)
		*(.bss*)
	}
	_end = .;

	/*
	 * Memory space reserved for kernel stack:
	 *  - one page to capture out-of-bounds addresses.
	 *  - kernel stack size.
	 */
	.kstack ALIGN(4096) : AT(ADDR(.kstack) - vaddr)
	{
		. = . + 4096 + kstack_size;
	}
	_kstack = .;

	/* remove information not needed */
	/DISCARD/ :
	{
		*(.eh_frame)
	}
}
