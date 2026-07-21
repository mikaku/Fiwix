OUTPUT_FORMAT("elf64-littleriscv")
OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
	. = 0x80000000;
	.text : ALIGN(4096)
	{
		KEEP(*(.text.boot))
		*(.text*)
	}
	.bss : ALIGN(4096)
	{
		*(.bss*)
	}
}
