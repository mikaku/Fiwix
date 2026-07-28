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
    .equ SYS_CHMOD,  15
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

run_linked_tree:
    ldr     r0, =linked_path
    movw    r1, #493
    mov     r7, #SYS_CHMOD
    svc     #0
    cmp     r0, #0
    blt     failed

    mov     r7, #SYS_FORK
    svc     #0
    cmp     r0, #0
    beq     run_linked
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
    mov     r1, #42
    lsl     r1, r1, #8
    cmp     r0, r1
    bne     linked_status_failed

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

run_linked:
    mov     r0, #1
    ldr     r1, =linked_start_message
    ldr     r2, =linked_start_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0

    ldr     r0, =linked_path
    ldr     r1, =linked_argv
    ldr     r2, =empty_envp
    mov     r7, #SYS_EXECVE
    svc     #0

    mov     r0, #2
    ldr     r1, =exec_failure_message
    ldr     r2, =exec_failure_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0
    b       failed

linked_status_failed:
    cmp     r0, #4
    ldreq   r1, =sigill_message
    ldreq   r2, =sigill_message_end
    beq     report_linked_failure
    cmp     r0, #7
    ldreq   r1, =sigbus_message
    ldreq   r2, =sigbus_message_end
    beq     report_linked_failure
    cmp     r0, #9
    ldreq   r1, =sigkill_message
    ldreq   r2, =sigkill_message_end
    beq     report_linked_failure
    cmp     r0, #11
    ldreq   r1, =sigsegv_message
    ldreq   r2, =sigsegv_message_end
    beq     report_linked_failure
    ldr     r1, =status_failure_message
    ldr     r2, =status_failure_message_end

report_linked_failure:
    sub     r2, r2, r1
    mov     r0, #2
    mov     r7, #SYS_WRITE
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
    .word   library_option
    .word   library_path
    .word   output_option
    .word   output_path
    .word   input_path
    .word   link_option
    .word   libc_name
    .word   0
mes_envp:
    .word   home_environment
    .word   path_environment
    .word   load_path_environment
    .word   prefix_environment
    .word   arena_environment
    .word   max_arena_environment
    .word   stack_environment
    .word   m1_environment
    .word   hex2_environment
    .word   0
linked_argv:
    .word   linked_path
    .word   0
empty_envp:
    .word   0

start_message:
    .ascii  "Fiwix ARM MesCC process tree entered\n"
start_message_end:
complete_message:
    .ascii  "Fiwix ARM MesCC process tree completed\n"
complete_message_end:
linked_start_message:
    .ascii  "Fiwix ARM MesCC linked ELF entered\n"
linked_start_message_end:
exec_failure_message:
    .ascii  "Fiwix ARM MesCC linked ELF exec returned\n"
exec_failure_message_end:
sigill_message:
    .ascii  "Fiwix ARM MesCC linked ELF exited on SIGILL\n"
sigill_message_end:
sigbus_message:
    .ascii  "Fiwix ARM MesCC linked ELF exited on SIGBUS\n"
sigbus_message_end:
sigkill_message:
    .ascii  "Fiwix ARM MesCC linked ELF exited on SIGKILL\n"
sigkill_message_end:
sigsegv_message:
    .ascii  "Fiwix ARM MesCC linked ELF exited on SIGSEGV\n"
sigsegv_message_end:
status_failure_message:
    .ascii  "Fiwix ARM MesCC linked ELF returned unexpected status\n"
status_failure_message_end:
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
library_option:
    .asciz  "-L"
library_path:
    .asciz  "/mes/lib"
input_path:
    .asciz  "hello.c"
output_option:
    .asciz  "-o"
output_path:
    .asciz  "hello"
link_option:
    .asciz  "-l"
libc_name:
    .asciz  "c"
linked_path:
    .asciz  "/hello"
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
m1_environment:
    .asciz  "M1=/bin/M1"
hex2_environment:
    .asciz  "HEX2=/bin/hex2"

    .ltorg
    .section .note.GNU-stack,"",%progbits
