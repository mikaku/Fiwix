/* Exit QEMU only after the nested stage0 kaem process returns successfully. */

	.option norvc
	.section .text
	.globl _start

	.equ SYS_WRITE, 64
	.equ SYS_EXIT, 93
	.equ SYS_REBOOT, 142
	.equ COMPLETION_MESSAGE_SIZE, 34

_start:
	li a0, 1
	.option push
	.option norelax
	lla a1, completion_message
	.option pop
	li a2, COMPLETION_MESSAGE_SIZE
	li a7, SYS_WRITE
	ecall

	li a0, 0xfee1dead
	li a1, 0x28121969
	li a2, 0x01234567
	li a7, SYS_REBOOT
	ecall

	li a0, 1
	li a7, SYS_EXIT
	ecall
1:
	j 1b

completion_message:
	.ascii "Fiwix riscv64 kaem mini completed\n"
completion_message_end:
	.if (completion_message_end - completion_message) != COMPLETION_MESSAGE_SIZE
	.error "COMPLETION_MESSAGE_SIZE is stale"
	.endif
