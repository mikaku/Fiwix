/*
 * fiwix/arch/arm/context.S
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

    .syntax unified
    .arch armv7-a
    .fpu vfpv3
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
    .equ ARCH_CONTEXT_FPSCR,     48
    .equ ARCH_CONTEXT_VFP_D,     56
    .equ ARCH_CONTEXT_SIZE,      312
    .equ ARM_FRAME_PC,           52
    .equ ARM_FRAME_CPSR,         56
    .equ ARM_FRAME_USR_SP,       60
    .equ ARM_FRAME_SIZE,         72

    .globl arm_arch_context_size
    .equ arm_arch_context_size, ARCH_CONTEXT_SIZE

    .text
    .globl arm_context_switch
    .globl arm_vfp_context_save
    .globl arm_context_gate_alternate
    .globl arm_context_gate_seed_vfp
    .globl arm_context_gate_check_primary_vfp
    .globl arm_kernel_process_entry
    .globl arm_user_process_entry
    .globl arm_return_to_user
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
    vmrs    r3, fpscr
    str     r3, [r0, #ARCH_CONTEXT_FPSCR]
    add     r3, r0, #ARCH_CONTEXT_VFP_D
    vstmia  r3!, {d0-d15}
    vstmia  r3!, {d16-d31}
    mov     r2, r1
    ldr     r3, [r2, #ARCH_CONTEXT_FPSCR]
    vmsr    fpscr, r3
    add     r3, r2, #ARCH_CONTEXT_VFP_D
    vldmia  r3!, {d0-d15}
    vldmia  r3!, {d16-d31}
    ldr     sp, [r2, #ARCH_CONTEXT_SP]
    ldr     lr, [r2, #ARCH_CONTEXT_LR]
    ldmia   r2, {r4-r11}
    bx      lr

arm_vfp_context_save:
    vmrs    r1, fpscr
    str     r1, [r0, #ARCH_CONTEXT_FPSCR]
    add     r1, r0, #ARCH_CONTEXT_VFP_D
    vstmia  r1!, {d0-d15}
    vstmia  r1!, {d16-d31}
    bx      lr

arm_kernel_process_entry:
    cpsie   i
    blx     r4
1:
    wfi
    b       1b

/* First entry for a generic process: r4=PC, r5=user SP. */
arm_user_process_entry:
    cpsid   if
    mov     r2, r4
    mov     r3, r5
    cps     #0x1f
    mov     sp, r3
    mov     lr, #0
    cps     #0x13
    mov     lr, r2
    mov     r0, #0x50
    msr     spsr_cxsf, r0
    movs    pc, lr

/* Resume a fork child from the copied struct arm_trap_frame at SP. */
arm_return_to_user:
    add     r0, sp, #ARM_FRAME_USR_SP
    ldmia   r0, {r13-r14}^
    nop
    ldr     r0, [sp, #ARM_FRAME_CPSR]
    msr     spsr_cxsf, r0
    ldr     lr, [sp, #ARM_FRAME_PC]
    ldmia   sp, {r0-r12}
    add     sp, sp, #ARM_FRAME_SIZE
    movs    pc, lr

/* Fixture-only gate; generic image linking discards this independent section. */
    .section .text.arm_context_gate, "ax"
arm_context_gate_seed_vfp:
    movw    r0, #0x3344
    movt    r0, #0x1122
    movw    r1, #0x7788
    movt    r1, #0x5566
    vmov    d0, r0, r1
    movw    r0, #0xbbcc
    movt    r0, #0x99aa
    movw    r1, #0xff00
    movt    r1, #0xddee
    vmov    d16, r0, r1
    bx      lr

arm_context_gate_check_primary_vfp:
    vmov    r0, r1, d0
    movw    r2, #0x3344
    movt    r2, #0x1122
    cmp     r0, r2
    bne     9f
    movw    r2, #0x7788
    movt    r2, #0x5566
    cmp     r1, r2
    bne     9f
    vmov    r0, r1, d16
    movw    r2, #0xbbcc
    movt    r2, #0x99aa
    cmp     r0, r2
    bne     9f
    movw    r2, #0xff00
    movt    r2, #0xddee
    cmp     r1, r2
    bne     9f
    mov     r0, #0
    bx      lr
9:
    mvn     r0, #0
    bx      lr

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
    vmov    r2, r3, d0
    orrs    r2, r2, r3
    bne     9f
    vmov    r2, r3, d16
    orrs    r2, r2, r3
    bne     9f
    movw    r2, #0xc0de
    movt    r2, #0x0bad
    movw    r3, #0x5678
    movt    r3, #0x1234
    vmov    d0, r2, r3
    movw    r2, #0x4321
    movt    r2, #0x8765
    movw    r3, #0xba98
    movt    r3, #0xfedc
    vmov    d16, r2, r3
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
    vmov    r2, r3, d0
    movw    r0, #0xc0de
    movt    r0, #0x0bad
    cmp     r2, r0
    bne     9f
    movw    r0, #0x5678
    movt    r0, #0x1234
    cmp     r3, r0
    bne     9f
    vmov    r2, r3, d16
    movw    r0, #0x4321
    movt    r0, #0x8765
    cmp     r2, r0
    bne     9f
    movw    r0, #0xba98
    movt    r0, #0xfedc
    cmp     r3, r0
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
