/* PID 1 launcher for the unmodified stage0-posix RV64 hex0 seed. */

	.option norvc
	.section .text
	.globl _start

	.equ SYS_WRITE, 64
	.equ SYS_EXIT, 93
	.equ SYS_EXECVE, 221
	.equ STAGE0_MESSAGE_SIZE, 38
	.equ FAILURE_MESSAGE_SIZE, 42

_start:
	li a0, 1
	.option push
	.option norelax
	lla a1, stage0_message
	.option pop
	li a2, STAGE0_MESSAGE_SIZE
	li a7, SYS_WRITE
	ecall

	addi sp, sp, -64
	lla t0, seed_path
	sd t0, 0(sp)
	lla t0, input_path
	sd t0, 8(sp)
	lla t0, output_path
	sd t0, 16(sp)
	sd zero, 24(sp)
	lla t0, init_home
	sd t0, 32(sp)
	sd zero, 40(sp)

	li a7, SYS_EXECVE
	lla a0, seed_path
	addi a1, sp, 0
	addi a2, sp, 32
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

stage0_message:
	.ascii "Fiwix riscv64 stage0 launcher entered\n"
stage0_message_end:
	.if (stage0_message_end - stage0_message) != STAGE0_MESSAGE_SIZE
	.error "STAGE0_MESSAGE_SIZE is stale"
	.endif
failure_message:
	.ascii "Fiwix riscv64 stage0 launcher exec failed\n"
failure_message_end:
	.if (failure_message_end - failure_message) != FAILURE_MESSAGE_SIZE
	.error "FAILURE_MESSAGE_SIZE is stale"
	.endif
seed_path:
	.asciz "/bin/hex0-seed"
input_path:
	.asciz "/stage0-input.hex0"
output_path:
	.asciz "/stage0-output"
init_home:
	.asciz "HOME=/"
