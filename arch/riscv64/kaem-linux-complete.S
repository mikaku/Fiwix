/* Hand the completed stage0 ext2 root to Linux through the reboot syscall. */

	.option norvc
	.section .text
	.globl _start

	.equ SYS_WRITE, 64
	.equ SYS_SYNC, 81
	.equ SYS_EXIT, 93
	.equ SYS_REBOOT, 142
	.equ LINUX_REBOOT_MAGIC1, 0xfee1dead
	.equ LINUX_REBOOT_MAGIC2, 0x28121969
	.equ LINUX_REBOOT_CMD_KEXEC, 0x45584543
	.equ COMPLETION_MESSAGE_SIZE, 33

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

	li a0, LINUX_REBOOT_MAGIC1
	li a1, LINUX_REBOOT_MAGIC2
	li a2, LINUX_REBOOT_CMD_KEXEC
	li a7, SYS_REBOOT
	ecall

	li a0, 1
	li a7, SYS_EXIT
	ecall
1:
	j 1b

completion_message:
	.ascii "Fiwix riscv64 kaem Linux handoff\n"
completion_message_end:
	.if (completion_message_end - completion_message) != COMPLETION_MESSAGE_SIZE
	.error "COMPLETION_MESSAGE_SIZE is stale"
	.endif
