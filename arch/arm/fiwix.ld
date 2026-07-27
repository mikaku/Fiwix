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

    .arm_user : ALIGN(4) {
        KEEP(*(.arm_user))
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
