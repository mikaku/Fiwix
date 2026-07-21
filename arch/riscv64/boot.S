/*
 * fiwix/arch/riscv64/boot.S
 *
 * Copyright 2026, Fiwix riscv64 contributors.
 * Distributed under the terms of the Fiwix License.
 */

	.option norvc
	.section .text.boot
	.globl _start
	.globl machine_trap
	.globl supervisor_entry
	.globl supervisor_trap

	.equ CLINT_MTIMECMP, 0x02004000
	.equ CLINT_MTIME,    0x0200bff8
	.equ TIMER_INTERVAL, 100000
	.equ MIP_SSIP,       0x2
	.equ MIE_MTIE,       0x80
	.equ MSTATUS_MPP,    0x1800
	.equ MSTATUS_MPP_S,  0x0800
	.equ SSTATUS_SIE,    0x2
	.equ SIE_SSIE,       0x2

_start:
	.option push
	.option norelax
	la gp, __global_pointer$
	.option pop
	la sp, __boot_stack_top
	andi sp, sp, -16

	/* Keep the QEMU boot contract available after the C call. */
	mv s0, a0
	mv s1, a1

	csrw mie, zero
	csrci mstatus, 8
	la t0, machine_trap
	csrw mtvec, t0

	mv a0, s0
	mv a1, s1
	call riscv64_machine_main

	/* Delegate synchronous U-mode traps and supervisor software interrupts. */
	li t0, 0xb1ff
	csrw medeleg, t0
	li t0, MIP_SSIP
	csrw mideleg, t0

	/* QEMU implements PMP, so S/U memory is denied until M mode grants it. */
	li t0, -1
	csrw pmpaddr0, t0
	li t0, 0x0f
	csrw pmpcfg0, t0

	/* Keep machine traps off the supervisor/user stack. */
	la t0, __machine_stack_top
	csrw mscratch, t0

	/* Arm the machine timer. It is forwarded to S mode as SSIP. */
	li t0, CLINT_MTIME
	ld t1, 0(t0)
	li t2, TIMER_INTERVAL
	add t1, t1, t2
	li t0, CLINT_MTIMECMP
	sd t1, 0(t0)
	li t0, MIE_MTIE
	csrw mie, t0

	csrr t0, mstatus
	li t1, MSTATUS_MPP
	not t1, t1
	and t0, t0, t1
	li t1, MSTATUS_MPP_S
	or t0, t0, t1
	csrw mstatus, t0
	la t0, supervisor_entry
	csrw mepc, t0
	mret

1:
	wfi
	j 1b

	.align 4
machine_trap:
	csrrw sp, mscratch, sp
	addi sp, sp, -32
	sd t0, 0(sp)
	sd t1, 8(sp)
	sd t2, 16(sp)

	csrr t0, mcause
	bgez t0, machine_trap_fatal
	slli t1, t0, 1
	srli t1, t1, 1
	li t2, 7
	bne t1, t2, machine_trap_fatal

	li t0, CLINT_MTIME
	ld t1, 0(t0)
	li t2, TIMER_INTERVAL
	add t1, t1, t2
	li t0, CLINT_MTIMECMP
	sd t1, 0(t0)
	li t0, MIP_SSIP
	csrs mip, t0

	ld t0, 0(sp)
	ld t1, 8(sp)
	ld t2, 16(sp)
	addi sp, sp, 32
	csrrw sp, mscratch, sp
	mret

machine_trap_fatal:
	csrr a0, mcause
	csrr a1, mepc
	csrr a2, mtval
	call riscv64_trap
2:
	wfi
	j 2b

	.align 4
supervisor_entry:
	la sp, __supervisor_stack_top
	andi sp, sp, -16
	la t0, supervisor_trap
	csrw stvec, t0
	li t0, SIE_SSIE
	csrw sie, t0
	csrs sstatus, t0
	mv a0, s0
	mv a1, s1
	call riscv64_supervisor_main
3:
	wfi
	j 3b

	.align 4
supervisor_trap:
	addi sp, sp, -32
	sd t0, 0(sp)
	sd t1, 8(sp)
	sd t2, 16(sp)
	csrr t0, scause
	bgez t0, supervisor_trap_fatal
	slli t1, t0, 1
	srli t1, t1, 1
	li t2, 1
	bne t1, t2, supervisor_trap_fatal

	li t0, MIP_SSIP
	csrc sip, t0
	la t0, riscv64_timer_ticks
	ld t1, 0(t0)
	addi t1, t1, 1
	sd t1, 0(t0)

	ld t0, 0(sp)
	ld t1, 8(sp)
	ld t2, 16(sp)
	addi sp, sp, 32
	sret

supervisor_trap_fatal:
	csrr a0, scause
	csrr a1, sepc
	csrr a2, stval
	call riscv64_supervisor_trap
4:
	wfi
	j 4b
