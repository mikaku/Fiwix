/* Position-independent Linux Image-format handoff fixture. */

	.option norvc
	.section .text
	.globl _start

_start:
	j payload_start
	nop
	.dword 0x00200000              /* text_offset for RV64 */
	.dword image_end - _start      /* image_size */
	.dword 0                       /* little-endian, 4 KiB pages */
	.word 2                        /* header version */
	.word 0
	.dword 0
	.dword 0x0000005643534952      /* "RISCV\0\0\0" */
	.word 0x05435352               /* "RSC\x05" */
	.word 0

payload_start:
	bnez a0, payload_failed
	beqz a1, payload_failed
	lbu t0, 0(a1)
	li t1, 0xd0
	bne t0, t1, payload_failed
	lbu t0, 1(a1)
	li t1, 0x0d
	bne t0, t1, payload_failed
	lbu t0, 2(a1)
	li t1, 0xfe
	bne t0, t1, payload_failed
	lbu t0, 3(a1)
	li t1, 0xed
	bne t0, t1, payload_failed
	csrr t0, satp
	bnez t0, payload_failed
	csrr t0, sie
	bnez t0, payload_failed

	.option push
	.option norelax
	lla t0, success_message
	.option pop
	li t1, 0x10000000
1:
	lbu t2, 0(t0)
	beqz t2, payload_pass
2:
	lbu t3, 5(t1)
	andi t3, t3, 0x20
	beqz t3, 2b
	sb t2, 0(t1)
	addi t0, t0, 1
	j 1b

payload_pass:
	li t0, 0x00100000
	li t1, 0x00005555
	sw t1, 0(t0)
3:
	wfi
	j 3b

payload_failed:
	li t0, 0x00100000
	li t1, 0x00013333
	sw t1, 0(t0)
4:
	wfi
	j 4b

success_message:
	.asciz "Fiwix riscv64 Linux Image handoff gate passed\n"
image_end:
