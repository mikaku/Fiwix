/*
 * fiwix/arch/arm/linux-root-init.S
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

    .syntax unified
    .arch armv7-a
    .arm
    .text
    .globl _start

    .equ SYS_WRITE,                    4
    .equ SYS_REBOOT,                  88
    .equ LINUX_REBOOT_MAGIC1, 0xfee1dead
    .equ LINUX_REBOOT_MAGIC2, 0x28121969
    .equ LINUX_REBOOT_CMD_KEXEC, 0x45584543
    .equ LINUX_REBOOT_CMD_POWER_OFF, 0x4321fedc

_start:
    ldr     r0, =LINUX_REBOOT_MAGIC1
    ldr     r1, =LINUX_REBOOT_MAGIC2
    ldr     r2, =LINUX_REBOOT_CMD_KEXEC
    mov     r7, #SYS_REBOOT
    svc     #0

    mov     r0, #1
    ldr     r1, =completion_message
    mov     r2, #COMPLETION_MESSAGE_SIZE
    mov     r7, #SYS_WRITE
    svc     #0
    cmp     r0, #COMPLETION_MESSAGE_SIZE
    bne     1f

    ldr     r0, =LINUX_REBOOT_MAGIC1
    ldr     r1, =LINUX_REBOOT_MAGIC2
    ldr     r2, =LINUX_REBOOT_CMD_POWER_OFF
    mov     r7, #SYS_REBOOT
    svc     #0
1:
    b       1b

completion_message:
    .ascii "Fiwix ARM Linux root PID 1 passed\n"
completion_message_end:
    .equ COMPLETION_MESSAGE_SIZE, completion_message_end - completion_message

    .ltorg
    .section .note.GNU-stack,"",%progbits
