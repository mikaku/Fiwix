/* Generic Fiwix supervisor and user trap entry for RV64. */

	.option norvc
	.section .text
	.globl riscv64_generic_trap_entry
	.globl riscv64_generic_traps_install

	.equ SIE_SSIE, 2
	.equ TRAP_FRAME_SIZE, 272

	.macro SAVE_FRAME from_kernel
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
	.if \from_kernel
	addi t0, sp, TRAP_FRAME_SIZE
	.else
	csrr t0, sscratch
	.endif
	sd t0, 8(sp)
	csrr t0, sepc
	sd t0, 248(sp)
	csrr t0, sstatus
	sd t0, 256(sp)
	csrr t0, stval
	sd t0, 264(sp)
	.endm

	.macro RESTORE_FRAME to_user
	ld t0, 248(sp)
	csrw sepc, t0
	ld t0, 256(sp)
	csrw sstatus, t0
	.if \to_user
	addi t0, sp, TRAP_FRAME_SIZE
	csrw sscratch, t0
	.else
	csrw sscratch, zero
	.endif
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
	.endm

riscv64_generic_traps_install:
	la t0, riscv64_generic_trap_entry
	csrw stvec, t0
	csrw sscratch, zero
	li t0, SIE_SSIE
	csrs sie, t0
	ret

	.balign 4
riscv64_generic_trap_entry:
	csrrw sp, sscratch, sp
	bnez sp, .Luser_trap
	csrrw sp, sscratch, sp
	SAVE_FRAME 1
	csrw sscratch, zero
	csrr a0, scause
	ld a1, 248(sp)
	ld a2, 264(sp)
	call riscv64_generic_kernel_trap
	bnez a0, .Lfatal
	RESTORE_FRAME 0

.Luser_trap:
	SAVE_FRAME 0
	csrw sscratch, zero
	mv a0, sp
	csrr a1, scause
	call riscv64_generic_user_trap
	bnez a0, .Lfatal
	RESTORE_FRAME 1

.Lfatal:
	csrr a0, scause
	ld a1, 248(sp)
	ld a2, 264(sp)
	call riscv64_generic_trap_fatal
1:
	wfi
	j 1b
