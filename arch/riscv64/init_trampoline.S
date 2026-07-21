/* Position-independent first userspace path for generic RV64 init. */

	.option norvc
	.section .text
	.globl riscv64_init_trampoline_start
	.globl riscv64_init_trampoline_end

	.equ SYS_OPENAT, 56
	.equ SYS_DUP, 23
	.equ SYS_EXECVE, 221
	.equ SYS_EXIT, 93
	.equ AT_FDCWD, -100

riscv64_init_trampoline_start:
	li a7, SYS_OPENAT
	li a0, AT_FDCWD
	lla a1, init_console
	li a2, 2
	ecall

	li a7, SYS_DUP
	li a0, 0
	ecall
	li a7, SYS_DUP
	li a0, 0
	ecall

	addi sp, sp, -48
	lla t0, init_path
	sd t0, 0(sp)
	sd zero, 8(sp)
	lla t0, init_home
	sd t0, 16(sp)
	lla t0, init_term
	sd t0, 24(sp)
	sd zero, 32(sp)

	li a7, SYS_EXECVE
	lla a0, init_path
	addi a1, sp, 0
	addi a2, sp, 16
	ecall

	li a7, SYS_EXIT
	li a0, 1
	ecall
1:
	j 1b

init_console:
	.asciz "/dev/console"
init_path:
	.asciz "/sbin/init"
init_home:
	.asciz "HOME=/"
init_term:
	.asciz "TERM=linux"
riscv64_init_trampoline_end:
