/* Standalone RV64 ELF fixture loaded by the bring-up kernel. */

	.option norvc
	.section .text
	.globl _start

	.equ USER_MESSAGE_SIZE, 42
	.equ STACK_MESSAGE_SIZE, 40
	.equ AT_PAGESZ, 6
	.equ AT_ENTRY, 9

_start:
	andi t0, sp, 15
	bnez t0, user_failed
	ld t0, 0(sp)
	li t1, 1
	bne t0, t1, user_failed
	ld t0, 8(sp)
	beqz t0, user_failed
	lbu t1, 0(t0)
	li t2, 'f'
	bne t1, t2, user_failed
	ld t0, 16(sp)
	bnez t0, user_failed
	ld t0, 24(sp)
	bnez t0, user_failed
	ld t0, 32(sp)
	li t1, AT_PAGESZ
	bne t0, t1, user_failed
	ld t0, 40(sp)
	li t1, 4096
	bne t0, t1, user_failed
	ld t0, 48(sp)
	li t1, AT_ENTRY
	bne t0, t1, user_failed
	ld t0, 56(sp)
	.option push
	.option norelax
	lla t1, _start
	.option pop
	bne t0, t1, user_failed
	ld t0, 64(sp)
	bnez t0, user_failed
	ld t0, 72(sp)
	bnez t0, user_failed

	li a0, 1
	.option push
	.option norelax
	lla a1, stack_message
	.option pop
	li a2, STACK_MESSAGE_SIZE
	li a7, 64
	ecall
	li t0, STACK_MESSAGE_SIZE
	bne a0, t0, user_failed

	li a0, 1
	.option push
	.option norelax
	lla a1, user_message
	.option pop
	li a2, USER_MESSAGE_SIZE
	li a7, 64
	ecall
	li t0, USER_MESSAGE_SIZE
	bne a0, t0, user_failed
	li a0, 42
	li a7, 93
	ecall
	ebreak

user_failed:
	li a0, 1
	li a7, 93
	ecall
	ebreak

stack_message:
	.ascii "Fiwix riscv64 initial stack gate passed\n"
stack_message_end:
	.if (stack_message_end - stack_message) != STACK_MESSAGE_SIZE
	.error "STACK_MESSAGE_SIZE is stale"
	.endif

user_message:
	.ascii "Fiwix riscv64 U-mode write syscall passed\n"
user_message_end:
	.if (user_message_end - user_message) != USER_MESSAGE_SIZE
	.error "USER_MESSAGE_SIZE is stale"
	.endif
