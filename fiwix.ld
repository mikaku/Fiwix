/*
 * Linker script for the Fiwix kernel.
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/linker.h>

OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH("i386")
ENTRY(_start)			/* entry point */
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

	/* remove information not needed */
	/DISCARD/ :
	{
		*(.eh_frame)
	}
}
