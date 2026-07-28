/*
 * fiwix/arch/arm/mes-init.S
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

    .syntax unified
    .arch armv7-a
    .arm
    .text
    .globl _start

    .equ SYS_EXIT,    1
    .equ SYS_FORK,    2
    .equ SYS_WRITE,   4
    .equ SYS_EXECVE, 11
    .equ SYS_SYNC,   36
    .equ SYS_REBOOT, 88
    .equ SYS_WAIT4, 114

    .equ REBOOT_MAGIC1, 0xfee1dead
    .equ REBOOT_MAGIC2, 0x28121969
    .equ REBOOT_COMMAND, 0x01234567

_start:
    mov     r0, #1
    ldr     r1, =start_message
    ldr     r2, =start_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0

    mov     r7, #SYS_FORK
    svc     #0
    cmp     r0, #0
    beq     run_mes
    blt     failed
    mov     r4, r0

    sub     sp, sp, #8
    mov     r0, r4
    mov     r1, sp
    mov     r2, #0
    mov     r3, #0
    mov     r7, #SYS_WAIT4
    svc     #0
    cmp     r0, r4
    bne     failed
    ldr     r0, [sp]
    add     sp, sp, #8
    cmp     r0, #0
    bne     failed

    mov     r0, #1
    ldr     r1, =complete_message
    ldr     r2, =complete_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0

    mov     r7, #SYS_SYNC
    svc     #0
    ldr     r0, =REBOOT_MAGIC1
    ldr     r1, =REBOOT_MAGIC2
    ldr     r2, =REBOOT_COMMAND
    mov     r7, #SYS_REBOOT
    svc     #0
    b       failed

run_mes:
    ldr     r0, =mes_path
    ldr     r1, =mes_argv
    ldr     r2, =mes_envp
    mov     r7, #SYS_EXECVE
    svc     #0

failed:
    mov     r0, #2
    ldr     r1, =failure_message
    ldr     r2, =failure_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0
    mov     r0, #1
    mov     r7, #SYS_EXIT
    svc     #0
1:
    b       1b

    .balign 4
mes_argv:
    .word   mes_path
    .word   command_option
    .word   scheme_expression
    .word   0
mes_envp:
    .word   home_environment
    .word   path_environment
    .word   load_path_environment
    .word   0

start_message:
    .ascii  "Fiwix ARM Mes process tree entered\n"
start_message_end:
complete_message:
    .ascii  "Fiwix ARM Mes process tree completed\n"
complete_message_end:
failure_message:
    .ascii  "Fiwix ARM Mes process tree failed\n"
failure_message_end:
mes_path:
    .asciz  "/mes-m2"
command_option:
    .asciz  "-c"
scheme_expression:
    .asciz  "(display \"Fiwix ARM Mes Scheme boundary passed\") (newline)"
home_environment:
    .asciz  "HOME=/"
path_environment:
    .asciz  "PATH=/bin:/sbin"
load_path_environment:
    .asciz  "GUILE_LOAD_PATH=/mes/module:/module"

    .ltorg
    .section .note.GNU-stack,"",%progbits
