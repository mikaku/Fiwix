/*
 * fiwix/arch/arm/boot.S
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

    .syntax unified
    .arch armv7-a
    .arch_extension virt
    .arch_extension sec
    .arm
    .section .text.boot
    .globl _start
    .globl arm_poweroff
    .globl arm_boot_dtb

_start:
    cpsid   if
    mrs     r0, cpsr
    and     r1, r0, #0x1f
    cmp     r1, #0x1a             /* HYP mode */
    bne     svc_entry

    mrc     p15, 4, r1, c14, c1, 0
    orr     r1, r1, #3           /* EL1 physical counter and timer access */
    mcr     p15, 4, r1, c14, c1, 0
    adr     r1, svc_entry
    msr     ELR_hyp, r1
    bic     r0, r0, #0x1f
    orr     r0, r0, #0x13         /* SVC mode */
    orr     r0, r0, #0xc0         /* IRQ and FIQ masked */
    msr     spsr_cxsf, r0
    eret

svc_entry:
    ldr     sp, =0x47fff000
    ldr     r3, =arm_boot_dtb
    str     r2, [r3]

    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    mov     r3, #0
1:  cmp     r0, r1
    strlo   r3, [r0], #4
    blo     1b

    bl      arm_boot_main
2:  wfi
    b       2b

arm_poweroff:
    ldr     r0, =0x84000008       /* PSCI SYSTEM_OFF */
    smc     #0
3:  wfi
    b       3b

    .section .data
    .align  2
arm_boot_dtb:
    .word   0

    .section .note.GNU-stack,"",%progbits
