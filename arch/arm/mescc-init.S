/*
 * fiwix/arch/arm/mescc-init.S
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
    beq     run_mescc
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

run_mescc:
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
    .word   no_auto_compile_option
    .word   entry_option
    .word   main_name
    .word   mescc_path
    .word   define_option
    .word   config_define
    .word   include_option
    .word   include_path
    .word   include_option
    .word   arm_include_path
    .word   assembly_option
    .word   input_path
    .word   output_option
    .word   output_path
    .word   0
mes_envp:
    .word   home_environment
    .word   path_environment
    .word   load_path_environment
    .word   prefix_environment
    .word   arena_environment
    .word   max_arena_environment
    .word   stack_environment
    .word   0

start_message:
    .ascii  "Fiwix ARM MesCC process tree entered\n"
start_message_end:
complete_message:
    .ascii  "Fiwix ARM MesCC process tree completed\n"
complete_message_end:
failure_message:
    .ascii  "Fiwix ARM MesCC process tree failed\n"
failure_message_end:
mes_path:
    .asciz  "/mes/bin/mes-m2"
no_auto_compile_option:
    .asciz  "--no-auto-compile"
entry_option:
    .asciz  "-e"
main_name:
    .asciz  "main"
mescc_path:
    .asciz  "/mes/mescc.scm"
define_option:
    .asciz  "-D"
config_define:
    .asciz  "HAVE_CONFIG_H=1"
include_option:
    .asciz  "-I"
include_path:
    .asciz  "/mes/include"
arm_include_path:
    .asciz  "/mes/include/linux/arm"
assembly_option:
    .asciz  "-S"
input_path:
    .asciz  "hello.c"
output_option:
    .asciz  "-o"
output_path:
    .asciz  "hello.s"
home_environment:
    .asciz  "HOME=/"
path_environment:
    .asciz  "PATH=/bin:/sbin"
load_path_environment:
    .asciz  "GUILE_LOAD_PATH=/mes/mes/module:/mes/module:/nyacc/module"
prefix_environment:
    .asciz  "MES_PREFIX=/mes"
arena_environment:
    .asciz  "MES_ARENA=20000000"
max_arena_environment:
    .asciz  "MES_MAX_ARENA=20000000"
stack_environment:
    .asciz  "MES_STACK=6000000"

    .ltorg
    .section .note.GNU-stack,"",%progbits
