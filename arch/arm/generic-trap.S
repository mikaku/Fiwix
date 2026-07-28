/*
 * fiwix/arch/arm/generic-trap.S
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

    .syntax unified
    .arch armv7-a
    .arm

    .equ FRAME_PC,      52
    .equ FRAME_CPSR,    56
    .equ FRAME_USR_SP,  60
    .equ FRAME_USR_LR,  64
    .equ FRAME_VECTOR,  68
    .equ FRAME_SIZE,    72

    .section .text.vectors, "ax"
    .align 5
    .globl arm_generic_vector_table
arm_generic_vector_table:
    b       arm_generic_unrecoverable
    b       arm_generic_undefined
    b       arm_generic_svc
    b       arm_generic_prefetch_abort
    b       arm_generic_data_abort
    b       arm_generic_unrecoverable
    b       arm_generic_irq
    b       arm_generic_fiq

    .text
    .globl arm_generic_traps_install
    .globl arm_generic_trap_frame_size
    .equ arm_generic_trap_frame_size, FRAME_SIZE
    .extern arm_generic_user_trap
    .extern arm_generic_kernel_trap
    .extern arm_generic_trap_fatal

arm_generic_traps_install:
    cpsid   if
    ldr     r0, =arm_generic_vector_table
    mcr     p15, 0, r0, c12, c0, 0
    mrc     p15, 0, r0, c1, c0, 0
    orr     r0, r0, #2
    mcr     p15, 0, r0, c1, c0, 0
    dsb
    isb
    bx      lr

/*
 * Non-SVC modes use SRS to place their adjusted return state on the current
 * process's banked SVC stack before switching modes. No global IRQ/abort
 * stack can therefore retain a schedulable user frame.
 */
.macro transfer_exception vector, adjustment
    cpsid   i
    sub     lr, lr, #\adjustment
    srsdb   sp!, #0x13
    cps     #0x13
    sub     sp, sp, #64
    stmia   sp, {r0-r12}
    ldr     r0, [sp, #64]
    str     r0, [sp, #FRAME_PC]
    ldr     r0, [sp, #68]
    str     r0, [sp, #FRAME_CPSR]
    and     r1, r0, #0x1f
    cmp     r1, #0x10
    bne     1f
    add     r0, sp, #FRAME_USR_SP
    stmia   r0, {r13-r14}^
    nop
    b       2f
1:
    add     r0, sp, #FRAME_SIZE
    str     r0, [sp, #FRAME_USR_SP]
    str     lr, [sp, #FRAME_USR_LR]
2:
    mov     r0, #\vector
    str     r0, [sp, #FRAME_VECTOR]
    b       arm_generic_dispatch
.endm

arm_generic_undefined:
    transfer_exception 1, 4

arm_generic_prefetch_abort:
    transfer_exception 3, 4

arm_generic_data_abort:
    transfer_exception 4, 8

arm_generic_irq:
    transfer_exception 6, 4

arm_generic_fiq:
    transfer_exception 7, 4

arm_generic_svc:
    cpsid   i
    sub     sp, sp, #FRAME_SIZE
    stmia   sp, {r0-r12}
    str     lr, [sp, #FRAME_PC]
    mrs     r0, spsr
    str     r0, [sp, #FRAME_CPSR]
    add     r0, sp, #FRAME_USR_SP
    stmia   r0, {r13-r14}^
    nop
    mov     r0, #2
    str     r0, [sp, #FRAME_VECTOR]

arm_generic_dispatch:
    ldr     r1, [sp, #FRAME_CPSR]
    and     r1, r1, #0x1f
    cmp     r1, #0x10
    bne     1f
    mov     r0, sp
    bl      arm_generic_user_trap
    b       2f
1:
    mov     r0, sp
    bl      arm_generic_kernel_trap
2:
    cmp     r0, #0
    bne     arm_generic_fatal

    ldr     r1, [sp, #FRAME_CPSR]
    and     r1, r1, #0x1f
    cmp     r1, #0x10
    bne     arm_generic_kernel_return

    add     r0, sp, #FRAME_USR_SP
    ldmia   r0, {r13-r14}^
    nop
    ldr     r0, [sp, #FRAME_CPSR]
    msr     spsr_cxsf, r0
    ldr     lr, [sp, #FRAME_PC]
    ldmia   sp, {r0-r12}
    add     sp, sp, #FRAME_SIZE
    movs    pc, lr

arm_generic_kernel_return:
    /*
     * Preserve the interrupted SVC link register and rebuild the exception
     * return pair in the final two words of this frame. RFE then restores the
     * CPSR while advancing SVC sp back to its pre-exception value.
     */
    ldr     r0, [sp, #FRAME_USR_LR]
    str     r0, [sp, #FRAME_USR_SP]
    ldr     r0, [sp, #FRAME_PC]
    str     r0, [sp, #FRAME_USR_LR]
    ldr     r0, [sp, #FRAME_CPSR]
    str     r0, [sp, #FRAME_VECTOR]
    ldmia   sp, {r0-r12}
    ldr     lr, [sp, #FRAME_USR_SP]
    add     sp, sp, #FRAME_USR_LR
    rfeia   sp!

arm_generic_fatal:
    mov     r0, sp
    bl      arm_generic_trap_fatal
arm_generic_unrecoverable:
1:
    wfi
    b       1b

    .section .note.GNU-stack,"",%progbits
