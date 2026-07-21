/* Enter a RISC-V Linux Image with a0=hart ID and a1=DTB physical address. */

	.option norvc
	.section .text
	.globl riscv64_linux_handoff

riscv64_linux_handoff:
	csrw sie, zero
	csrci sstatus, 2
	csrw sscratch, zero
	csrw satp, zero
	sfence.vma zero, zero
	fence.i
	mv t0, a0
	mv a0, a1
	mv a1, a2
	jr t0
