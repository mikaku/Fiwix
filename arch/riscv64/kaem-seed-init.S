/* PID 1 launcher for a stage0-posix RV64 kaem script. */

	.option norvc
	.section .text
	.globl _start

	.equ SYS_WRITE, 64
	.equ SYS_EXIT, 93
	.equ SYS_EXECVE, 221
	.equ KAEM_MESSAGE_SIZE, 41
	.equ FAILURE_MESSAGE_SIZE, 45

_start:
	li a0, 1
	.option push
	.option norelax
	lla a1, kaem_message
	.option pop
	li a2, KAEM_MESSAGE_SIZE
	li a7, SYS_WRITE
	ecall

	addi sp, sp, -64
	lla t0, kaem_path
	sd t0, 0(sp)
	lla t0, script_path
	sd t0, 8(sp)
	sd zero, 16(sp)
	lla t0, init_home
	sd t0, 24(sp)
	lla t0, init_path
	sd t0, 32(sp)
	sd zero, 40(sp)

	li a7, SYS_EXECVE
	lla a0, kaem_path
	addi a1, sp, 0
	addi a2, sp, 24
	ecall

	li a0, 2
	lla a1, failure_message
	li a2, FAILURE_MESSAGE_SIZE
	li a7, SYS_WRITE
	ecall
	li a0, 1
	li a7, SYS_EXIT
	ecall
1:
	j 1b

kaem_message:
	.ascii "Fiwix riscv64 kaem seed launcher entered\n"
kaem_message_end:
	.if (kaem_message_end - kaem_message) != KAEM_MESSAGE_SIZE
	.error "KAEM_MESSAGE_SIZE is stale"
	.endif
failure_message:
	.ascii "Fiwix riscv64 kaem seed launcher exec failed\n"
failure_message_end:
	.if (failure_message_end - failure_message) != FAILURE_MESSAGE_SIZE
	.error "FAILURE_MESSAGE_SIZE is stale"
	.endif
kaem_path:
	.asciz "/bootstrap-seeds/POSIX/riscv64/kaem-optional-seed"
script_path:
	.asciz "/riscv64/mescc-tools-seed-kaem.kaem"
init_home:
	.asciz "HOME=/"
init_path:
	.asciz "PATH=/bin:/sbin"
