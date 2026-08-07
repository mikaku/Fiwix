/* Filesystem-backed PID 1 fixture for the generic RV64 kernel. */

	.option norvc
	.section .text
	.globl _start

	.equ MESSAGE_SIZE, 37

_start:
	andi t0, sp, 15
	bnez t0, init_failed
	ld t0, 0(sp)
	li t1, 1
	bne t0, t1, init_failed
	ld t0, 8(sp)
	beqz t0, init_failed
	lbu t1, 0(t0)
	li t2, '/'
	bne t1, t2, init_failed

	li a0, 1
	.option push
	.option norelax
	lla a1, init_message
	.option pop
	li a2, MESSAGE_SIZE
	li a7, 64
	ecall
	li t0, MESSAGE_SIZE
	bne a0, t0, init_failed

1:
	li a7, 172
	ecall
	j 1b

init_failed:
	li a0, 1
	li a7, 93
	ecall
	ebreak

init_message:
	.ascii "Fiwix riscv64 PID 1 userspace passed\n"
init_message_end:
	.if (init_message_end - init_message) != MESSAGE_SIZE
	.error "MESSAGE_SIZE is stale"
	.endif
