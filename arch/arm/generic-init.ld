/*
 * fiwix/arch/arm/generic-init.ld
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

PHDRS
{
	text PT_LOAD FILEHDR PHDRS FLAGS(5);
}

SECTIONS
{
	. = 0x00400000 + SIZEOF_HEADERS;
	.text :
	{
		*(.text*)
	} :text

	/DISCARD/ :
	{
		*(.comment)
		*(.ARM.attributes)
	}
}
