/* SPDX-License-Identifier: GPL-2.0-or-later */

    .syntax unified
    .arch armv7-a
    .arm

    .equ ARCH_CONTEXT_R4,         0
    .equ ARCH_CONTEXT_R5,         4
    .equ ARCH_CONTEXT_R6,         8
    .equ ARCH_CONTEXT_R7,        12
    .equ ARCH_CONTEXT_R8,        16
    .equ ARCH_CONTEXT_R9,        20
    .equ ARCH_CONTEXT_R10,       24
    .equ ARCH_CONTEXT_R11,       28
    .equ ARCH_CONTEXT_SP,        32
    .equ ARCH_CONTEXT_LR,        36
    .equ ARCH_CONTEXT_TTBR0,     40
    .equ ARCH_CONTEXT_KERNEL_SP, 44
    .equ ARCH_CONTEXT_SIZE,      48

    .globl arm_arch_context_size
    .equ arm_arch_context_size, ARCH_CONTEXT_SIZE

    .text
    .globl arm_context_switch
    .globl arm_context_gate_alternate
    .extern arm_context_gate_primary_context
    .extern arm_context_gate_alternate_context
    .extern arm_context_gate_phase

/*
 * r0=outgoing context, r1=incoming context. TTBR0 activation is separate so
 * the scheduler can order the address-space switch before changing current.
 */
arm_context_switch:
    stmia   r0, {r4-r11}
    str     sp, [r0, #ARCH_CONTEXT_SP]
    str     lr, [r0, #ARCH_CONTEXT_LR]
    mov     r2, r1
    ldr     sp, [r2, #ARCH_CONTEXT_SP]
    ldr     lr, [r2, #ARCH_CONTEXT_LR]
    ldmia   r2, {r4-r11}
    bx      lr

/* Two resumptions on an independent stack exercise save and restore. */
arm_context_gate_alternate:
    cmp     r4, #0x44
    bne     9f
    cmp     r5, #0x55
    bne     9f
    cmp     r6, #0x66
    bne     9f
    cmp     r7, #0x77
    bne     9f
    cmp     r8, #0x88
    bne     9f
    cmp     r9, #0x99
    bne     9f
    cmp     r10, #0xaa
    bne     9f
    cmp     r11, #0xbb
    bne     9f
    ldr     r2, =arm_context_gate_phase
    mov     r3, #1
    str     r3, [r2]
    ldr     r0, =arm_context_gate_alternate_context
    ldr     r1, =arm_context_gate_primary_context
    bl      arm_context_switch

    cmp     r4, #0x44
    bne     9f
    cmp     r5, #0x55
    bne     9f
    cmp     r6, #0x66
    bne     9f
    cmp     r7, #0x77
    bne     9f
    cmp     r8, #0x88
    bne     9f
    cmp     r9, #0x99
    bne     9f
    cmp     r10, #0xaa
    bne     9f
    cmp     r11, #0xbb
    bne     9f
    ldr     r2, =arm_context_gate_phase
    mov     r3, #2
    str     r3, [r2]
    b       8f

9:
    ldr     r2, =arm_context_gate_phase
    mvn     r3, #0
    str     r3, [r2]
8:
    ldr     r0, =arm_context_gate_alternate_context
    ldr     r1, =arm_context_gate_primary_context
    bl      arm_context_switch
7:
    wfi
    b       7b

    .section .note.GNU-stack,"",%progbits
