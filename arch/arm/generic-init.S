/*
 * fiwix/arch/arm/generic-init.S
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

    .syntax unified
    .arch armv7-a
    .arm
    .text
    .globl _start

    .equ SYS_EXIT,          1
    .equ SYS_FORK,          2
    .equ SYS_WRITE,         4
    .equ SYS_GETPID,       20
    .equ SYS_KILL,         37
    .equ SYS_REBOOT,       88
    .equ SYS_WAIT4,       114
    .equ SYS_RT_SIGACTION, 174

    .equ SIGUSR1,          10
    .equ SIGSET_SIZE,       8
    .equ CHILD_STATUS, 0x2a00
    .equ REBOOT_MAGIC1, 0xfee1dead
    .equ REBOOT_MAGIC2, 0x28121969
    .equ REBOOT_COMMAND, 0x01234567

_start:
    tst     sp, #7
    bne     init_failed
    ldr     r0, [sp]
    cmp     r0, #1
    bne     init_failed
    ldr     r0, [sp, #4]
    cmp     r0, #0
    beq     init_failed
    ldrb    r0, [r0]
    cmp     r0, #'/'
    bne     init_failed

    sub     sp, sp, #24
    ldr     r0, =signal_handler
    str     r0, [sp, #0]
    mov     r0, #0
    str     r0, [sp, #4]
    str     r0, [sp, #8]
    str     r0, [sp, #12]
    str     r0, [sp, #16]
    mov     r0, #SIGUSR1
    mov     r1, sp
    mov     r2, #0
    mov     r3, #SIGSET_SIZE
    mov     r7, #SYS_RT_SIGACTION
    svc     #0
    cmp     r0, #0
    bne     init_failed
    add     sp, sp, #24

    mov     r7, #SYS_GETPID
    svc     #0
    cmp     r0, #1
    bne     init_failed
    mov     r4, r0
    mov     r1, #SIGUSR1
    mov     r7, #SYS_KILL
    svc     #0
    cmp     r0, #0
    bne     init_failed

    mov     r7, #SYS_FORK
    svc     #0
    cmp     r0, #0
    beq     init_child
    blt     init_failed
    mov     r4, r0

    sub     sp, sp, #8
    mov     r0, r4
    mov     r1, sp
    mov     r2, #0
    mov     r3, #0
    mov     r7, #SYS_WAIT4
    svc     #0
    cmp     r0, r4
    bne     init_failed
    ldr     r0, [sp]
    add     sp, sp, #8
    ldr     r1, =CHILD_STATUS
    cmp     r0, r1
    bne     init_failed

    mov     r0, #1
    ldr     r1, =wait_message
    mov     r2, #WAIT_MESSAGE_SIZE
    mov     r7, #SYS_WRITE
    svc     #0
    cmp     r0, #WAIT_MESSAGE_SIZE
    bne     init_failed

    mov     r0, #1
    ldr     r1, =init_message
    mov     r2, #INIT_MESSAGE_SIZE
    mov     r7, #SYS_WRITE
    svc     #0
    cmp     r0, #INIT_MESSAGE_SIZE
    bne     init_failed

    ldr     r0, =REBOOT_MAGIC1
    ldr     r1, =REBOOT_MAGIC2
    ldr     r2, =REBOOT_COMMAND
    mov     r7, #SYS_REBOOT
    svc     #0
    b       init_failed

init_child:
    sub     sp, sp, #8
    mov     r0, #0x5a
    str     r0, [sp]
    mov     r0, #1
    ldr     r1, =child_message
    mov     r2, #CHILD_MESSAGE_SIZE
    mov     r7, #SYS_WRITE
    svc     #0
    cmp     r0, #CHILD_MESSAGE_SIZE
    bne     init_failed
    mov     r0, #42
    mov     r7, #SYS_EXIT
    svc     #0
    b       init_failed

signal_handler:
    cmp     r0, #SIGUSR1
    bne     init_failed
    mov     r0, #1
    ldr     r1, =signal_message
    mov     r2, #SIGNAL_MESSAGE_SIZE
    mov     r7, #SYS_WRITE
    svc     #0
    cmp     r0, #SIGNAL_MESSAGE_SIZE
    bne     init_failed
    bx      lr

init_failed:
    mov     r0, #2
    ldr     r1, =failure_message
    mov     r2, #FAILURE_MESSAGE_SIZE
    mov     r7, #SYS_WRITE
    svc     #0
    mov     r0, #1
    mov     r7, #SYS_EXIT
    svc     #0
1:
    b       1b

signal_message:
    .ascii "Fiwix ARM PID 1 signal passed\n"
signal_message_end:
    .equ SIGNAL_MESSAGE_SIZE, signal_message_end - signal_message

child_message:
    .ascii "Fiwix ARM PID 1 child COW passed\n"
child_message_end:
    .equ CHILD_MESSAGE_SIZE, child_message_end - child_message

wait_message:
    .ascii "Fiwix ARM PID 1 fork and wait4 passed\n"
wait_message_end:
    .equ WAIT_MESSAGE_SIZE, wait_message_end - wait_message

init_message:
    .ascii "Fiwix ARM PID 1 userspace passed\n"
init_message_end:
    .equ INIT_MESSAGE_SIZE, init_message_end - init_message

failure_message:
    .ascii "Fiwix ARM PID 1 userspace failed\n"
failure_message_end:
    .equ FAILURE_MESSAGE_SIZE, failure_message_end - failure_message

    .ltorg
    .section .note.GNU-stack,"",%progbits
