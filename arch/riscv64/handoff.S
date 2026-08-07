/* Enter a RISC-V Linux Image with a0=hart ID and a1=DTB physical address. */

	.option norvc
	.section .text
	.globl riscv64_linux_handoff
	.globl riscv64_sbi_gate

	.equ SBI_EXT_BASE, 0x10
	.equ SBI_EXT_TIME, 0x54494d45
	.equ SBI_EXT_SRST, 0x53525354

riscv64_sbi_gate:
	li a7, SBI_EXT_BASE
	li a6, 0
	ecall
	bnez a0, 1f
	li t0, 3
	bne a1, t0, 1f
	li a7, SBI_EXT_BASE
	li a6, 3
	li a0, SBI_EXT_TIME
	ecall
	bnez a0, 1f
	li t0, 1
	bne a1, t0, 1f
	li a7, SBI_EXT_BASE
	li a6, 3
	li a0, SBI_EXT_SRST
	ecall
	bnez a0, 1f
	li t0, 1
	bne a1, t0, 1f
	rdtime t0
	li t1, 10000
	add a0, t0, t1
	li a7, SBI_EXT_TIME
	li a6, 0
	ecall
	bnez a0, 1f
	li t2, 10000000
2:
	csrr t0, sip
	andi t0, t0, 0x20
	bnez t0, 3f
	addi t2, t2, -1
	bnez t2, 2b
	j 1f
3:
	li a0, -1
	li a7, SBI_EXT_TIME
	li a6, 0
	ecall
	bnez a0, 1f
	li a0, 0
	ret
1:
	li a0, -1
	ret

riscv64_linux_handoff:
	csrw sie, zero
	csrci sstatus, 2
	csrw sscratch, zero
	csrw satp, zero
	sfence.vma zero, zero
	li t0, 0x02004000
	li t1, -1
	sd t1, 0(t0)
	li t0, 0x22
	csrc sip, t0
	fence.i
	mv t0, a0
	mv a0, a1
	mv a1, a2
	jr t0
