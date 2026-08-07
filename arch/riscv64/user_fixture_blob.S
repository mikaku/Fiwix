/* Embed the standalone ELF as immutable input to the kernel loader gate. */

	.section .rodata.user_fixture,"a",@progbits
	.balign 8
	.globl riscv64_user_elf_start
	.globl riscv64_user_elf_end
riscv64_user_elf_start:
	.incbin "fixture/user.elf"
riscv64_user_elf_end:
