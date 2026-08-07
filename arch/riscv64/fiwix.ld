/* Linker script for firmware-free QEMU virt bring-up on riscv64. */

OUTPUT_FORMAT("elf64-littleriscv")
OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
	. = 0x80000000;
	__kernel_start = .;

	.text : ALIGN(4096)
	{
		KEEP(*(.text.boot))
		*(.text*)
	}

	.rodata : ALIGN(4096)
	{
		*(.rodata*)
	}

	.data : ALIGN(4096)
	{
		PROVIDE(__global_pointer$ = . + 0x800);
		*(.sdata*)
		*(.data*)
	}

	.bss : ALIGN(4096)
	{
		__bss_start = .;
		*(.sbss*)
		*(.bss*)
		*(COMMON)
		__bss_end = .;
		. = ALIGN(16);
		. += 16384;
		__supervisor_stack_top = .;
		. += 8192;
		__supervisor_trap_stack_top = .;
		. += 4096;
		__machine_stack_top = .;
		__boot_stack_top = __machine_stack_top;
	}

	ASSERT(. <= 0x80200000, "Fiwix overlaps the RV64 Linux Image load address")
	. = 0x80200000;
	.linux_image (NOLOAD) : ALIGN(0x200000)
	{
		*(.linux_image)
	}

	__kernel_end = .;

	/DISCARD/ :
	{
		*(.comment)
		*(.eh_frame*)
		*(.riscv.attributes)
	}
}
