/*
 * fiwix/arch/arm/tcc-init.S
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
    beq     run_tcc
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

run_tcc:
    ldr     r0, =tcc_path
    ldr     r1, =tcc_argv
    ldr     r2, =process_envp
    mov     r7, #SYS_EXECVE
    svc     #0
    b       exec_failed

run_linked:
    mov     r0, #1
    ldr     r1, =linked_start_message
    ldr     r2, =linked_start_message_end
    sub     r2, r2, r1
    mov     r7, #SYS_WRITE
    svc     #0

    ldr     r0, =linked_path
    ldr     r1, =linked_argv
    ldr     r2, =process_envp
    mov     r7, #SYS_EXECVE
    svc     #0

exec_failed:
    mov     r0, #2
    ldr     r1, =exec_failure_message
    ldr     r2, =exec_failure_message_end
    sub     r2, r2, r1
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
tcc_argv:
    .word   tcc_path
    .word   no_stdlib_option
    .word   static_option
    .word   text_address_option
    .word   output_option
    .word   output_path
    .word   include_option
    .word   include_path
    .word   include_option
    .word   arm_include_path
    .word   crt1_path
    .word   crti_path
    .word   input_path
    .word   libc_path
    .word   libtcc1_path
    .word   crtn_path
    .word   0
linked_argv:
    .word   linked_path
    .word   0
process_envp:
    .word   home_environment
    .word   path_environment
    .word   0

start_message:
    .ascii  "Fiwix ARM TinyCC process tree entered\n"
start_message_end:
complete_message:
    .ascii  "Fiwix ARM TinyCC process tree completed\n"
complete_message_end:
linked_start_message:
    .ascii  "Fiwix ARM TinyCC linked ELF entered\n"
linked_start_message_end:
exec_failure_message:
    .ascii  "Fiwix ARM TinyCC exec returned\n"
exec_failure_message_end:
failure_message:
    .ascii  "Fiwix ARM TinyCC process tree failed\n"
failure_message_end:
tcc_path:
    .asciz  "/bin/tcc-mes"
no_stdlib_option:
    .asciz  "-nostdlib"
static_option:
    .asciz  "-static"
text_address_option:
    .asciz  "-Wl,-Ttext=0x10074"
output_option:
    .asciz  "-o"
output_path:
    .asciz  "hello"
include_option:
    .asciz  "-I"
include_path:
    .asciz  "/mes/include"
arm_include_path:
    .asciz  "/mes/include/linux/arm"
crt1_path:
    .asciz  "/mes/lib/crt1.o"
crti_path:
    .asciz  "/mes/lib/crti.o"
input_path:
    .asciz  "hello.c"
libc_path:
    .asciz  "/mes/lib/libc.a"
libtcc1_path:
    .asciz  "/mes/lib/tcc/libtcc1.a"
crtn_path:
    .asciz  "/mes/lib/crtn.o"
linked_path:
    .asciz  "/hello"
home_environment:
    .asciz  "HOME=/"
path_environment:
    .asciz  "PATH=/bin:/sbin"

    .ltorg
    .section .note.GNU-stack,"",%progbits
