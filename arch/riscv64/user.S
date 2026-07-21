/*
 * fiwix/arch/riscv64/user.S
 *
 * Copyright 2026, Fiwix riscv64 contributors.
 * Distributed under the terms of the Fiwix License.
 */

	.option norvc

	.section .text
	.globl riscv64_enter_user
	.globl riscv64_user_trap_entry

	.equ SSTATUS_SIE,  0x2
	.equ SSTATUS_SPIE, 0x20
	.equ SSTATUS_SPP,  0x100
	.equ TRAP_FRAME_SIZE, 272

/* Enter U mode and retain the supervisor call continuation for sys_exit. */
riscv64_enter_user:
	csrci sstatus, SSTATUS_SIE
	la t0, user_kernel_sp
	sd sp, 0(t0)
	la t0, user_kernel_ra
	sd ra, 0(t0)
	csrw sepc, a0
	csrr t0, sstatus
	li t1, SSTATUS_SPP
	not t1, t1
	and t0, t0, t1
	ori t0, t0, SSTATUS_SPIE
	csrw sstatus, t0
	la t0, __supervisor_trap_stack_top
	csrw sscratch, t0
	mv sp, a1
	sret

/*
 * sscratch contains the dedicated kernel trap stack on U-mode entry. The
 * initial swap is also the privilege test used by boot.S: a zero result means
 * the trap interrupted supervisor code and must use its existing stack.
 */
riscv64_user_trap_entry:
	addi sp, sp, -TRAP_FRAME_SIZE
	sd ra, 0(sp)
	sd gp, 16(sp)
	sd tp, 24(sp)
	sd t0, 32(sp)
	sd t1, 40(sp)
	sd t2, 48(sp)
	sd s0, 56(sp)
	sd s1, 64(sp)
	sd a0, 72(sp)
	sd a1, 80(sp)
	sd a2, 88(sp)
	sd a3, 96(sp)
	sd a4, 104(sp)
	sd a5, 112(sp)
	sd a6, 120(sp)
	sd a7, 128(sp)
	sd s2, 136(sp)
	sd s3, 144(sp)
	sd s4, 152(sp)
	sd s5, 160(sp)
	sd s6, 168(sp)
	sd s7, 176(sp)
	sd s8, 184(sp)
	sd s9, 192(sp)
	sd s10, 200(sp)
	sd s11, 208(sp)
	sd t3, 216(sp)
	sd t4, 224(sp)
	sd t5, 232(sp)
	sd t6, 240(sp)
	csrr t0, sscratch
	sd t0, 8(sp)
	csrr t0, sepc
	sd t0, 248(sp)
	csrr t0, sstatus
	sd t0, 256(sp)
	csrr t0, stval
	sd t0, 264(sp)

	mv a0, sp
	csrr a1, scause
	call riscv64_user_trap
	bnez a0, riscv64_leave_user

	ld t0, 248(sp)
	csrw sepc, t0
	ld t0, 256(sp)
	csrw sstatus, t0
	la t0, __supervisor_trap_stack_top
	csrw sscratch, t0

	ld ra, 0(sp)
	ld gp, 16(sp)
	ld tp, 24(sp)
	ld t0, 32(sp)
	ld t1, 40(sp)
	ld t2, 48(sp)
	ld s0, 56(sp)
	ld s1, 64(sp)
	ld a0, 72(sp)
	ld a1, 80(sp)
	ld a2, 88(sp)
	ld a3, 96(sp)
	ld a4, 104(sp)
	ld a5, 112(sp)
	ld a6, 120(sp)
	ld a7, 128(sp)
	ld s2, 136(sp)
	ld s3, 144(sp)
	ld s4, 152(sp)
	ld s5, 160(sp)
	ld s6, 168(sp)
	ld s7, 176(sp)
	ld s8, 184(sp)
	ld s9, 192(sp)
	ld s10, 200(sp)
	ld s11, 208(sp)
	ld t3, 216(sp)
	ld t4, 224(sp)
	ld t5, 232(sp)
	ld t6, 240(sp)
	ld sp, 8(sp)
	sret

riscv64_leave_user:
	csrw sscratch, zero
	la t0, riscv64_user_exit_status
	ld a0, 0(t0)
	la t0, user_kernel_sp
	ld sp, 0(t0)
	la t0, user_kernel_ra
	ld ra, 0(t0)
	csrsi sstatus, SSTATUS_SIE
	ret

	.section .bss
	.balign 8
user_kernel_sp:
	.dword 0
user_kernel_ra:
	.dword 0
