/* Linux PID 1 that re-verifies stage0 on the ext2 root prepared by Fiwix. */

	.option norvc
	.section .text
	.globl _start

	.equ SYS_WRITE, 64
	.equ SYS_EXECVE, 221
	.equ SYS_REBOOT, 142
	.equ LINUX_REBOOT_MAGIC1, 0xfee1dead
	.equ LINUX_REBOOT_MAGIC2, 0x28121969
	.equ LINUX_REBOOT_CMD_POWER_OFF, 0x4321fedc
	.equ CONTINUATION_MESSAGE_SIZE, 48
	.equ FAILURE_MESSAGE_SIZE, 52

_start:
	li a0, 1
	.option push
	.option norelax
	lla a1, continuation_message
	.option pop
	li a2, CONTINUATION_MESSAGE_SIZE
	li a7, SYS_WRITE
	ecall

	addi sp, sp, -96
	lla t0, kaem_path
	sd t0, 0(sp)
	lla t0, verbose_argument
	sd t0, 8(sp)
	lla t0, strict_argument
	sd t0, 16(sp)
	lla t0, file_argument
	sd t0, 24(sp)
	lla t0, script_path
	sd t0, 32(sp)
	sd zero, 40(sp)
	lla t0, init_home
	sd t0, 48(sp)
	lla t0, init_path
	sd t0, 56(sp)
	sd zero, 64(sp)

	li a7, SYS_EXECVE
	lla a0, kaem_path
	addi a1, sp, 0
	addi a2, sp, 48
	ecall

	li a0, 2
	lla a1, failure_message
	li a2, FAILURE_MESSAGE_SIZE
	li a7, SYS_WRITE
	ecall
	li a0, LINUX_REBOOT_MAGIC1
	li a1, LINUX_REBOOT_MAGIC2
	li a2, LINUX_REBOOT_CMD_POWER_OFF
	li a7, SYS_REBOOT
	ecall
1:
	j 1b

continuation_message:
	.ascii "Fiwix riscv64 Linux stage0 continuation entered\n"
continuation_message_end:
	.if (continuation_message_end - continuation_message) != CONTINUATION_MESSAGE_SIZE
	.error "CONTINUATION_MESSAGE_SIZE is stale"
	.endif
failure_message:
	.ascii "Fiwix riscv64 Linux stage0 continuation exec failed\n"
failure_message_end:
	.if (failure_message_end - failure_message) != FAILURE_MESSAGE_SIZE
	.error "FAILURE_MESSAGE_SIZE is stale"
	.endif
kaem_path:
	.asciz "/riscv64/bin/kaem"
verbose_argument:
	.asciz "--verbose"
strict_argument:
	.asciz "--strict"
file_argument:
	.asciz "--file"
script_path:
	.asciz "/steps/live-bootstrap-linux.kaem"
init_home:
	.asciz "HOME=/"
init_path:
	.asciz "PATH=/riscv64/bin:/usr/bin:/bin:/sbin"
