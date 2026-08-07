/*
 * fiwix/arch/arm/handoff.S
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

    .syntax unified
    .arch armv7-a
    .arm
    .text
    .globl arm_linux_handoff

/*
 * Enter a Linux zImage in SVC mode with r0=0, r1=~0, and r2 holding
 * the physical address of the copied device tree.
 */
arm_linux_handoff:
    mov     r4, r0
    mov     r5, r1
    cpsid   if

    mov     r0, #0
    mcr     p15, 0, r0, c14, c2, 1
    dsb

    mrc     p15, 0, r1, c1, c0, 0
    bic     r1, r1, #5
    mcr     p15, 0, r1, c1, c0, 0
    isb

    mcr     p15, 0, r0, c8, c7, 0
    mcr     p15, 0, r0, c7, c5, 0
    mcr     p15, 0, r0, c7, c5, 6
    dsb
    isb

    mov     r0, #0
    mvn     r1, #0
    mov     r2, r5
    bx      r4

    .section .note.GNU-stack,"",%progbits
