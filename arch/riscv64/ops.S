/* Privileged and ordering operations kept outside compiler inline assembly. */

	.option norvc
	.section .text
	.equ ARCH_CONTEXT_SATP, 112
	.globl riscv64_wait_for_interrupt
	.globl riscv64_clear_ssip
	.globl riscv64_vm_install
	.globl riscv64_fence_write
	.globl riscv64_fence_full
	.globl riscv64_fence_read
	.globl riscv64_fence_i
	.globl riscv64_interrupt_disable
	.globl riscv64_interrupt_enable
	.globl riscv64_interrupt_state
	.globl riscv64_interrupt_restore
	.globl riscv64_read_stval
	.globl riscv64_read_sp
	.globl riscv64_set_sp
	.globl riscv64_user_syscall3
	.globl riscv64_kernel_process_entry
	.globl riscv64_user_process_entry
	.globl riscv64_return_to_user
	.globl riscv64_context_activate
	.globl riscv64_read_satp
	.globl riscv64_make_satp
	.globl invalidate_tlb

riscv64_wait_for_interrupt:
	wfi
	ret

riscv64_clear_ssip:
	csrci sip, 2
	ret

riscv64_vm_install:
	li t0, 8
	slli t0, t0, 60
	or a0, a0, t0
	csrw satp, a0
	sfence.vma zero, zero
	li t0, 0x40000
	csrs sstatus, t0
	ret

riscv64_fence_write:
	fence w, w
	ret

riscv64_fence_full:
	fence rw, rw
	ret

riscv64_fence_read:
	fence r, r
	ret

riscv64_fence_i:
	fence.i
	ret

riscv64_interrupt_disable:
	csrci sstatus, 2
	ret

riscv64_interrupt_enable:
	csrsi sstatus, 2
	ret

riscv64_interrupt_state:
	csrr a0, sstatus
	ret

riscv64_interrupt_restore:
	andi a0, a0, 2
	beqz a0, 1f
	csrsi sstatus, 2
	ret
1:
	csrci sstatus, 2
	ret

riscv64_read_stval:
	csrr a0, stval
	ret

riscv64_read_sp:
	mv a0, sp
	ret

riscv64_set_sp:
	mv sp, a0
	ret

riscv64_user_syscall3:
	mv a7, a0
	mv a0, a1
	mv a1, a2
	mv a2, a3
	ecall
	ret

riscv64_read_satp:
	csrr a0, satp
	ret

riscv64_make_satp:
	li t0, 8
	slli t0, t0, 60
	or a0, a0, t0
	ret

invalidate_tlb:
	sfence.vma zero, zero
	ret

/* Activate a complete process SATP value and reset kernel trap selection. */
riscv64_context_activate:
	ld t0, ARCH_CONTEXT_SATP(a0)
	csrr t1, satp
	beq t0, t1, 1f
	csrw satp, t0
	sfence.vma zero, zero
1:
	csrw sscratch, zero
	ret

riscv64_kernel_process_entry:
	csrsi sstatus, 2
	jalr ra, s0, 0
1:
	wfi
	j 1b

/* First entry for a generic userspace process: s0=PC, s1=user SP. */
riscv64_user_process_entry:
	csrci sstatus, 2
	csrw sscratch, sp
	csrw sepc, s0
	csrr t0, sstatus
	li t1, 0x100
	not t1, t1
	and t0, t0, t1
	ori t0, t0, 0x20
	csrw sstatus, t0
	mv sp, s1
	sret

/* Resume a fork child from its copied trap frame; s0 is its kernel stack top. */
riscv64_return_to_user:
	csrw sscratch, s0
	ld t0, 248(sp)
	csrw sepc, t0
	ld t0, 256(sp)
	csrw sstatus, t0
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
