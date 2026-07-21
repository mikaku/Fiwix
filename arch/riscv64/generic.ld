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
	_etext = .;

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
	_edata = .;

	.bss (NOLOAD) : ALIGN(4096)
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
	ASSERT(. <= 0x80200000,
		"Fiwix generic RV64 image overlaps Linux load address")
	. = 0x80200000;
	.linux_image (NOLOAD) : ALIGN(0x200000)
	{
		*(.linux_image)
	}

	_end = .;
	__kernel_end = .;

	ASSERT(. <= 0x81000000, "Fiwix generic RV64 image exceeds 16 MiB")

	/DISCARD/ :
	{
		*(.comment)
		*(.eh_frame*)
		*(.riscv.attributes)
	}
}
