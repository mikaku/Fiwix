/* Exit only after live-bootstrap's first package installs successfully. */

	.option norvc
	.section .text
	.globl _start

	.equ SYS_WRITE, 64
	.equ SYS_SYNC, 81
	.equ SYS_EXIT, 93
	.equ SYS_REBOOT, 142
	.equ COMPLETION_MESSAGE_SIZE, 50

_start:
	li a0, 1
	.option push
	.option norelax
	lla a1, completion_message
	.option pop
	li a2, COMPLETION_MESSAGE_SIZE
	li a7, SYS_WRITE
	ecall

	li a7, SYS_SYNC
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
	.ascii "Fiwix riscv64 live-bootstrap manifest 1 completed\n"
completion_message_end:
	.if (completion_message_end - completion_message) != COMPLETION_MESSAGE_SIZE
	.error "COMPLETION_MESSAGE_SIZE is stale"
	.endif
