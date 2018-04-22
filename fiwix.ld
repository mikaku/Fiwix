/*
 * Linker script for the Fiwix kernel (3GB user / 1GB kernel).
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(start)		/* entry point */
vaddr = 0xC0000000;	/* virtual base address at 3GB */
paddr = 0x100000;	/* physical address at 1MB */

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
