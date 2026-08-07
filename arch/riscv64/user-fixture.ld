OUTPUT_FORMAT("elf64-littleriscv")
OUTPUT_ARCH("riscv")
ENTRY(_start)

PHDRS
{
	text PT_LOAD FLAGS(5);
}

SECTIONS
{
	. = 0x00400000;
	.text :
	{
		*(.text*)
	} :text

	/DISCARD/ :
	{
		*(.comment)
		*(.eh_frame*)
		*(.riscv.attributes)
	}
}
