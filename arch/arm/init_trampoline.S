/*
 * fiwix/arch/arm/init_trampoline.S
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

    .syntax unified
    .arch armv7-a
    .arm
    .text

    .equ SYS_EXIT,   1
    .equ SYS_OPEN,   5
    .equ SYS_EXECVE, 11
    .equ SYS_DUP,    41

    .globl arm_init_trampoline_start
    .globl arm_init_trampoline_end

arm_init_trampoline_start:
    adr     r0, init_console
    mov     r1, #2
    mov     r2, #0
    mov     r7, #SYS_OPEN
    svc     #0

    mov     r0, #0
    mov     r7, #SYS_DUP
    svc     #0
    mov     r0, #0
    mov     r7, #SYS_DUP
    svc     #0

    sub     sp, sp, #24
    adr     r3, init_path
    str     r3, [sp, #0]
    mov     r3, #0
    str     r3, [sp, #4]
    adr     r3, init_home
    str     r3, [sp, #8]
    adr     r3, init_term
    str     r3, [sp, #12]
    mov     r3, #0
    str     r3, [sp, #16]

    adr     r0, init_path
    mov     r1, sp
    add     r2, sp, #8
    mov     r7, #SYS_EXECVE
    svc     #0

    mov     r0, #1
    mov     r7, #SYS_EXIT
    svc     #0
1:
    b       1b

init_console:
    .asciz  "/dev/console"
init_path:
    .asciz  "/sbin/init"
init_home:
    .asciz  "HOME=/"
init_term:
    .asciz  "TERM=linux"
    .balign 4
arm_init_trampoline_end:

    .section .note.GNU-stack,"",%progbits
