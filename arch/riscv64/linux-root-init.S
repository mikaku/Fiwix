/* Linux PID 1 fixture for the Fiwix-to-Linux ext2 root handoff. */

	.option norvc
	.section .text
	.globl _start

	.equ SYS_WRITE, 64
	.equ SYS_REBOOT, 142
	.equ LINUX_REBOOT_MAGIC1, 0xfee1dead
	.equ LINUX_REBOOT_MAGIC2, 0x28121969
	.equ LINUX_REBOOT_CMD_POWER_OFF, 0x4321fedc
	.equ MESSAGE_SIZE, 38

_start:
	li a0, 1
	.option push
	.option norelax
	lla a1, completion_message
	.option pop
	li a2, MESSAGE_SIZE
	li a7, SYS_WRITE
	ecall
	li t0, MESSAGE_SIZE
	bne a0, t0, 1f

	li a0, LINUX_REBOOT_MAGIC1
	li a1, LINUX_REBOOT_MAGIC2
	li a2, LINUX_REBOOT_CMD_POWER_OFF
	li a7, SYS_REBOOT
	ecall
1:
	j 1b

completion_message:
	.ascii "Fiwix riscv64 Linux root PID 1 passed\n"
completion_message_end:
	.if (completion_message_end - completion_message) != MESSAGE_SIZE
	.error "MESSAGE_SIZE is stale"
	.endif
