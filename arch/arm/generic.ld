/*
 * fiwix/arch/arm/generic.ld
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

OUTPUT_ARCH(arm)
ENTRY(_start)

PHDRS
{
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
}

SECTIONS
{
	. = 0x40010000;
	__kernel_start = .;

	.text : ALIGN(4096) {
		KEEP(*(.text.boot))
		*(.text*)
	} :text
	_etext = .;

	.rodata : ALIGN(4096) {
		*(.rodata*)
	} :text

	.data : ALIGN(4096) {
		*(.data*)
	} :data
	_edata = .;

	.bss (NOLOAD) : ALIGN(4096) {
		__bss_start = .;
		*(.bss*)
		*(COMMON)
		__bss_end = .;
	} :data

	_end = .;
	__kernel_end = .;

	ASSERT(. <= 0x42000000, "Fiwix generic ARM image exceeds 32 MiB")

	/DISCARD/ : {
		*(.comment)
		*(.eh_frame*)
		*(.note*)
		*(.ARM.exidx*)
		*(.ARM.extab*)
		*(.ARM.attributes)
	}
}
