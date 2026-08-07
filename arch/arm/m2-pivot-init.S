/*
 * fiwix/arch/arm/m2-pivot-init.S
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
    beq     run_m2
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

run_m2:
    ldr     r0, =m2_path
    ldr     r1, =m2_argv
    ldr     r2, =m2_envp
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
m2_argv:
    .word   m2_path
    .word   architecture_option
    .word   architecture_name
    .word   file_option
    .word   types_path
    .word   file_option
    .word   stddef_path
    .word   file_option
    .word   unistd_path
    .word   file_option
    .word   arm_fcntl_path
    .word   file_option
    .word   fcntl_path
    .word   file_option
    .word   stdarg_path
    .word   file_option
    .word   string_path
    .word   file_option
    .word   ctype_path
    .word   file_option
    .word   stdlib_path
    .word   file_option
    .word   stdio_header_path
    .word   file_option
    .word   stdio_source_path
    .word   file_option
    .word   bootstrappable_path
    .word   file_option
    .word   hello_path
    .word   output_option
    .word   output_path
    .word   0
m2_envp:
    .word   home_environment
    .word   path_environment
    .word   0

start_message:
    .ascii  "Fiwix ARM M2 pivot process tree entered\n"
start_message_end:
complete_message:
    .ascii  "Fiwix ARM M2 pivot process tree completed\n"
complete_message_end:
failure_message:
    .ascii  "Fiwix ARM M2 pivot process tree failed\n"
failure_message_end:
m2_path:
    .asciz  "/M2-Planet-armv7l"
architecture_option:
    .asciz  "--architecture"
architecture_name:
    .asciz  "armv7l"
file_option:
    .asciz  "-f"
types_path:
    .asciz  "/M2libc/sys/types.h"
stddef_path:
    .asciz  "/M2libc/stddef.h"
unistd_path:
    .asciz  "/M2libc/armv7l/linux/unistd.c"
arm_fcntl_path:
    .asciz  "/M2libc/armv7l/linux/fcntl.c"
fcntl_path:
    .asciz  "/M2libc/fcntl.c"
stdarg_path:
    .asciz  "/M2libc/stdarg.h"
string_path:
    .asciz  "/M2libc/string.c"
ctype_path:
    .asciz  "/M2libc/ctype.c"
stdlib_path:
    .asciz  "/M2libc/stdlib.c"
stdio_header_path:
    .asciz  "/M2libc/stdio.h"
stdio_source_path:
    .asciz  "/M2libc/stdio.c"
bootstrappable_path:
    .asciz  "/M2libc/bootstrappable.c"
hello_path:
    .asciz  "/arm32-pivot-hello.c"
output_option:
    .asciz  "-o"
output_path:
    .asciz  "/M2-output.M1"
home_environment:
    .asciz  "HOME=/"
path_environment:
    .asciz  "PATH=/bin:/sbin"

    .ltorg
    .section .note.GNU-stack,"",%progbits
