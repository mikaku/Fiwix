/* Privileged and ordering operations kept outside compiler inline assembly. */

	.option norvc
	.section .text
	.globl riscv64_wait_for_interrupt
	.globl riscv64_clear_ssip
	.globl riscv64_vm_install
	.globl riscv64_fence_write
	.globl riscv64_fence_full
	.globl riscv64_fence_read

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
