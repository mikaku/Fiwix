/*
 * fiwix/arch/arm/ops.S
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

    .syntax unified
    .arch armv7-a
    .arm

    .text
    .globl arm_interrupt_disable
    .globl arm_interrupt_enable
    .globl arm_interrupt_state
    .globl arm_interrupt_restore
    .globl arm_read_dfar
    .globl arm_read_dfsr
    .globl arm_read_ifar
    .globl arm_read_ifsr
    .globl arm_read_sp
    .globl arm_set_sp
    .globl arm_wait_for_interrupt
    .globl arm_instruction_cache_invalidate
    .globl arm_user_syscall3
    .globl invalidate_tlb

arm_interrupt_disable:
    cpsid   i
    bx      lr

arm_interrupt_enable:
    cpsie   i
    bx      lr

arm_interrupt_state:
    mrs     r0, cpsr
    bx      lr

arm_interrupt_restore:
    tst     r0, #0x80
    beq     1f
    cpsid   i
    bx      lr
1:
    cpsie   i
    bx      lr

arm_read_dfar:
    mrc     p15, 0, r0, c6, c0, 0
    bx      lr

arm_read_dfsr:
    mrc     p15, 0, r0, c5, c0, 0
    bx      lr

arm_read_ifar:
    mrc     p15, 0, r0, c6, c0, 2
    bx      lr

arm_read_ifsr:
    mrc     p15, 0, r0, c5, c0, 1
    bx      lr

arm_read_sp:
    mov     r0, sp
    bx      lr

arm_set_sp:
    mov     sp, r0
    bx      lr

arm_wait_for_interrupt:
    wfi
    bx      lr

arm_instruction_cache_invalidate:
    mov     r0, #0
    dsb
    mcr     p15, 0, r0, c7, c5, 0
    mcr     p15, 0, r0, c7, c5, 6
    dsb
    isb
    bx      lr

arm_user_syscall3:
    push    {r7, lr}
    mov     r7, r0
    mov     r0, r1
    mov     r1, r2
    mov     r2, r3
    svc     #0
    pop     {r7, pc}

invalidate_tlb:
    mov     r0, #0
    dsb
    mcr     p15, 0, r0, c8, c7, 0
    dsb
    isb
    bx      lr

    .section .note.GNU-stack,"",%progbits
