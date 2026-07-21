	.option norvc
	.section .text.boot
	.globl _start

	.equ TEST_FINISHER, 0x00100000
	.equ TEST_PASS, 0x00005555
	.equ TEST_FAIL, 0x00013333
	.equ MSTATUS_MPP, 0x1800
	.equ MSTATUS_MPP_S, 0x0800
	.equ SSTATUS_SPP, 0x100
	.equ SSTATUS_SPIE, 0x20

_start:
	la sp, machine_stack_top
	la t0, machine_fatal
	csrw mtvec, t0
	li t0, -1
	csrw pmpaddr0, t0
	li t0, 0x0f
	csrw pmpcfg0, t0
	li t0, 1 << 8
	csrw medeleg, t0
	csrr t0, mstatus
	li t1, MSTATUS_MPP
	not t1, t1
	and t0, t0, t1
	li t1, MSTATUS_MPP_S
	or t0, t0, t1
	csrw mstatus, t0
	la t0, supervisor_start
	csrw mepc, t0
	mret

machine_fatal:
	li t0, TEST_FINISHER
	li t1, TEST_FAIL
	sw t1, 0(t0)
1:
	wfi
	j 1b

supervisor_start:
	la sp, supervisor_stack_top
	call riscv64_generic_traps_install
	la t0, user_start
	csrw sepc, t0
	csrr t0, sstatus
	li t1, SSTATUS_SPP
	not t1, t1
	and t0, t0, t1
	ori t0, t0, SSTATUS_SPIE
	csrw sstatus, t0
	la t0, kernel_stack_top
	csrw sscratch, t0
	la sp, user_stack_top
	sret

user_start:
	li ra, 0x101
	li gp, 0x102
	li tp, 0x103
	li t0, 0x104
	li t1, 0x105
	li t2, 0x106
	li s0, 0x107
	li s11, 0x108
	li a0, 0x109
	li a7, 0x10a
	li t3, 0x10b
	li t6, 0x10c
user_ecall:
	ecall
user_resumed:
	li t0, 0x1234
	bne a0, t0, user_fail
	li t0, TEST_FINISHER
	li t1, TEST_PASS
	sw t1, 0(t0)
user_fail:
	li t0, TEST_FINISHER
	li t1, TEST_FAIL
	sw t1, 0(t0)
2:
	j 2b

	.section .text
	.globl riscv64_generic_user_trap
riscv64_generic_user_trap:
	li t0, 8
	bne a1, t0, trap_fail
	mv t6, a0
	ld t0, 0(t6)
	li t1, 0x101
	bne t0, t1, trap_fail
	ld t0, 16(t6)
	li t1, 0x102
	bne t0, t1, trap_fail
	ld t0, 24(t6)
	li t1, 0x103
	bne t0, t1, trap_fail
	ld t0, 32(t6)
	li t1, 0x104
	bne t0, t1, trap_fail
	ld t0, 40(t6)
	li t1, 0x105
	bne t0, t1, trap_fail
	ld t0, 48(t6)
	li t1, 0x106
	bne t0, t1, trap_fail
	ld t0, 56(t6)
	li t1, 0x107
	bne t0, t1, trap_fail
	ld t0, 208(t6)
	li t1, 0x108
	bne t0, t1, trap_fail
	ld t0, 72(t6)
	li t1, 0x109
	bne t0, t1, trap_fail
	ld t0, 128(t6)
	li t1, 0x10a
	bne t0, t1, trap_fail
	ld t0, 216(t6)
	li t1, 0x10b
	bne t0, t1, trap_fail
	ld t0, 240(t6)
	li t1, 0x10c
	bne t0, t1, trap_fail
	ld t0, 8(t6)
	la t1, user_stack_top
	bne t0, t1, trap_fail
	ld t0, 248(t6)
	la t1, user_ecall
	bne t0, t1, trap_fail
	addi t0, t0, 4
	sd t0, 248(t6)
	li t0, 0x1234
	sd t0, 72(t6)
	li a0, 0
	ret

	.globl riscv64_generic_kernel_trap
riscv64_generic_kernel_trap:
	li a0, -1
	ret

	.globl riscv64_generic_trap_fatal
riscv64_generic_trap_fatal:
trap_fail:
	li t0, TEST_FINISHER
	li t1, TEST_FAIL
	sw t1, 0(t0)
3:
	wfi
	j 3b

	.section .bss
	.balign 16
	.space 4096
machine_stack_top:
	.space 4096
supervisor_stack_top:
	.space 4096
kernel_stack_top:
	.space 4096
user_stack_top:
