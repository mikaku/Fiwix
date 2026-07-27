/*
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

    .syntax unified
    .arch armv7-a
    .arm

    .equ UART_BASE,      0x09000000
    .equ UART_FR,        0x18
    .equ UART_TX_FULL,   0x20
    .equ FRAME_PC,       52
    .equ FRAME_CPSR,     56
    .equ FRAME_USR_SP,   60
    .equ FRAME_USR_LR,   64
    .equ FRAME_VECTOR,   68

    .text
    .globl arm_boot_main
    .globl arm_generic_user_trap
    .globl arm_generic_kernel_trap
    .globl arm_generic_trap_fatal
    .extern arm_generic_traps_install
    .extern arm_poweroff

arm_boot_main:
    bl      arm_generic_traps_install
    ldr     r2, =user_start
    ldr     r3, =user_stack_top
    cps     #0x1f
    mov     sp, r3
    movw    lr, #0x10e
    cps     #0x13
    mov     lr, r2
    mov     r0, #0x50
    msr     spsr_cxsf, r0
    movs    pc, lr

user_start:
    movw    r0, #0x101
    movw    r1, #0x102
    movw    r2, #0x103
    movw    r3, #0x104
    movw    r4, #0x105
    movw    r5, #0x106
    movw    r6, #0x107
    movw    r7, #0x108
    movw    r8, #0x109
    movw    r9, #0x10a
    movw    r10, #0x10b
    movw    r11, #0x10c
    movw    r12, #0x10d
user_svc:
    svc     #0
user_resumed:
    movw    lr, #0x1234
    cmp     r0, lr
    bne     user_fail
    movw    lr, #0x102
    cmp     r1, lr
    bne     user_fail
    movw    lr, #0x107
    cmp     r6, lr
    bne     user_fail
    movw    lr, #0x109
    cmp     r8, lr
    bne     user_fail
    movw    lr, #0x10d
    cmp     r12, lr
    bne     user_fail
    mov     r7, #0xee
    svc     #0
user_fail:
    mov     r7, #0xef
    svc     #0
1:
    b       1b

.macro check_word offset, value
    ldr     r1, [r3, #\offset]
    movw    r2, #\value
    cmp     r1, r2
    bne     trap_fail
.endm

arm_generic_user_trap:
    mov     r3, r0
    ldr     r1, [r3, #28]
    cmp     r1, #0xee
    beq     trap_pass
    cmp     r1, #0xef
    beq     trap_fail
    check_word 0, 0x101
    check_word 4, 0x102
    check_word 8, 0x103
    check_word 12, 0x104
    check_word 16, 0x105
    check_word 20, 0x106
    check_word 24, 0x107
    check_word 28, 0x108
    check_word 32, 0x109
    check_word 36, 0x10a
    check_word 40, 0x10b
    check_word 44, 0x10c
    check_word 48, 0x10d
    ldr     r1, [r3, #FRAME_PC]
    ldr     r2, =user_resumed
    cmp     r1, r2
    bne     trap_fail
    ldr     r1, [r3, #FRAME_CPSR]
    and     r1, r1, #0x1f
    cmp     r1, #0x10
    bne     trap_fail
    ldr     r1, [r3, #FRAME_USR_SP]
    ldr     r2, =user_stack_top
    cmp     r1, r2
    bne     trap_fail
    check_word FRAME_USR_LR, 0x10e
    check_word FRAME_VECTOR, 2
    movw    r1, #0x1234
    str     r1, [r3]
    mov     r0, #0
    bx      lr

arm_generic_kernel_trap:
    mvn     r0, #0
    bx      lr

arm_generic_trap_fatal:
trap_fail:
    ldr     r0, =failure_message
    bl      uart_puts
    b       arm_poweroff

trap_pass:
    ldr     r0, =success_message
    bl      uart_puts
    b       arm_poweroff

uart_puts:
    push    {r4-r6, lr}
    mov     r4, r0
    ldr     r5, =UART_BASE
1:
    ldrb    r6, [r4], #1
    cmp     r6, #0
    beq     3f
2:
    ldr     r0, [r5, #UART_FR]
    tst     r0, #UART_TX_FULL
    bne     2b
    strb    r6, [r5]
    b       1b
3:
    pop     {r4-r6, pc}

    .section .rodata
success_message:
    .asciz  "Fiwix ARM generic trap runtime passed\n"
failure_message:
    .asciz  "Fiwix ARM generic trap runtime failed\n"

    .section .bss
    .align 3
    .space 4096
user_stack_top:

    .section .note.GNU-stack,"",%progbits
