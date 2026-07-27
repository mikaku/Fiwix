/*
 * fiwix/arch/arm/fiwix.ld
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
    _kernel_start = .;

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
    } :text

    .rodata : {
        *(.rodata*)
    } :text

    .arm_elf_fixture : ALIGN(4) {
        KEEP(*(.arm_elf_fixture))
    } :text

    . = ALIGN(0x1000);
    .data : {
        *(.data*)
    } :data

    . = ALIGN(16);
    __bss_start = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    } :data
    __bss_end = .;
    _kernel_end = .;
}
