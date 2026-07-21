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
	.equ MIP_STIP,       0x20
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

	/* Keep S-mode ecalls in M mode for SBI; delegate U traps and S timers. */
	li t0, 0xb1ff
	csrw medeleg, t0
	li t0, MIP_SSIP | MIP_STIP
	csrw mideleg, t0
	li t0, 7
	csrw mcounteren, t0

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
	addi sp, sp, -128
	sd ra, 0(sp)
	sd t0, 8(sp)
	sd t1, 16(sp)
	sd t2, 24(sp)
	sd t3, 32(sp)
	sd t4, 40(sp)
	sd t5, 48(sp)
	sd t6, 56(sp)
	sd a0, 64(sp)
	sd a1, 72(sp)
	sd a2, 80(sp)
	sd a3, 88(sp)
	sd a4, 96(sp)
	sd a5, 104(sp)
	sd a6, 112(sp)
	sd a7, 120(sp)

	csrr t0, mcause
	bgez t0, machine_trap_exception
	slli t1, t0, 1
	srli t1, t1, 1
	li t2, 7
	bne t1, t2, machine_trap_fatal

	la t0, sbi_timer_active
	ld t1, 0(t0)
	beqz t1, machine_timer_fiwix
	li t0, CLINT_MTIMECMP
	li t1, -1
	sd t1, 0(t0)
	li t0, MIP_STIP
	csrs mip, t0
	j machine_trap_return

machine_timer_fiwix:
	li t0, CLINT_MTIME
	ld t1, 0(t0)
	li t2, TIMER_INTERVAL
	add t1, t1, t2
	li t0, CLINT_MTIMECMP
	sd t1, 0(t0)
	li t0, MIP_SSIP
	csrs mip, t0
	j machine_trap_return

machine_trap_exception:
	li t1, 9
	bne t0, t1, machine_trap_fatal
	ld t0, 120(sp)                 /* SBI extension ID */
	ld t1, 112(sp)                 /* SBI function ID */
	li t2, 0x10
	beq t0, t2, sbi_base
	li t2, 0x54494d45             /* TIME */
	beq t0, t2, sbi_time
	li t2, 0x53525354             /* SRST */
	beq t0, t2, sbi_reset
	beqz t0, sbi_legacy_set_timer
	li t2, 1
	beq t0, t2, sbi_legacy_putchar
	li t2, 2
	beq t0, t2, sbi_legacy_getchar
	li t2, 8
	beq t0, t2, sbi_legacy_shutdown
	j sbi_not_supported

sbi_base:
	beqz t1, sbi_base_spec_version
	li t2, 1
	beq t1, t2, sbi_base_impl_id
	li t2, 2
	beq t1, t2, sbi_base_impl_version
	li t2, 3
	beq t1, t2, sbi_base_probe
	li t2, 4
	beq t1, t2, sbi_base_mvendorid
	li t2, 5
	beq t1, t2, sbi_base_marchid
	li t2, 6
	beq t1, t2, sbi_base_mimpid
	j sbi_not_supported

sbi_base_spec_version:
	li t2, 3                       /* SBI specification 0.3 */
	j sbi_success_value
sbi_base_impl_id:
	li t2, 0x46495758              /* "FIWX" private implementation */
	j sbi_success_value
sbi_base_impl_version:
	li t2, 1
	j sbi_success_value
sbi_base_probe:
	ld t2, 64(sp)
	li t0, 0x54494d45
	beq t2, t0, 1f
	li t0, 0x53525354
	beq t2, t0, 1f
	li t2, 0
	j sbi_success_value
1:
	li t2, 1
	j sbi_success_value
sbi_base_mvendorid:
	csrr t2, mvendorid
	j sbi_success_value
sbi_base_marchid:
	csrr t2, marchid
	j sbi_success_value
sbi_base_mimpid:
	csrr t2, mimpid
	j sbi_success_value

sbi_time:
	bnez t1, sbi_not_supported
sbi_legacy_set_timer:
	ld t2, 64(sp)
	li t0, CLINT_MTIMECMP
	sd t2, 0(t0)
	li t0, MIP_STIP
	csrc mip, t0
	la t0, sbi_timer_active
	li t2, 1
	sd t2, 0(t0)
	li t2, 0
	j sbi_success_value

sbi_reset:
	bnez t1, sbi_not_supported
sbi_legacy_shutdown:
	li t0, 0x00100000
	li t1, 0x00005555
	sw t1, 0(t0)
	j sbi_legacy_shutdown

sbi_legacy_putchar:
	ld t2, 64(sp)
	li t0, 0x10000000
2:
	lbu t1, 5(t0)
	andi t1, t1, 0x20
	beqz t1, 2b
	sb t2, 0(t0)
	li t2, 0
	j sbi_legacy_value

sbi_legacy_getchar:
	li t2, -1
	j sbi_legacy_value

sbi_not_supported:
	li t2, -2                     /* SBI_ERR_NOT_SUPPORTED */
	sd t2, 64(sp)
	sd zero, 72(sp)
	j sbi_advance

sbi_success_value:
	sd zero, 64(sp)
	sd t2, 72(sp)
	j sbi_advance

sbi_legacy_value:
	sd t2, 64(sp)

sbi_advance:
	csrr t0, mepc
	addi t0, t0, 4
	csrw mepc, t0

machine_trap_return:
	ld ra, 0(sp)
	ld t0, 8(sp)
	ld t1, 16(sp)
	ld t2, 24(sp)
	ld t3, 32(sp)
	ld t4, 40(sp)
	ld t5, 48(sp)
	ld t6, 56(sp)
	ld a0, 64(sp)
	ld a1, 72(sp)
	ld a2, 80(sp)
	ld a3, 88(sp)
	ld a4, 96(sp)
	ld a5, 104(sp)
	ld a6, 112(sp)
	ld a7, 120(sp)
	addi sp, sp, 128
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
	csrw sscratch, zero
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
	csrrw sp, sscratch, sp
	bnez sp, riscv64_user_trap_entry
	csrrw sp, sscratch, sp

supervisor_kernel_trap:
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

	.section .bss
	.balign 8
sbi_timer_active:
	.dword 0
