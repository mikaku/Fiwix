/*
 * fiwix/arch/arm/traps.S
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

    .syntax unified
    .arch armv7-a
    .arch_extension virt
    .arm

    .equ SVC_STACK_TOP, 0x47fff000
    .equ IRQ_STACK_TOP, 0x47ffe000
    .equ ABT_STACK_TOP, 0x47ffd000
    .equ UND_STACK_TOP, 0x47ffc000
    .equ FIQ_STACK_TOP, 0x47ffb000
    .equ USR_STACK_TOP, 0x47ffa000

    .equ FRAME_PC,      52
    .equ FRAME_CPSR,    56
    .equ FRAME_USR_SP,  60
    .equ FRAME_USR_LR,  64
    .equ FRAME_VECTOR,  68
    .equ FRAME_SIZE,    72

    .section .text.vectors, "ax"
    .align 5
    .globl arm_vector_table
arm_vector_table:
    b       arm_reset_vector
    b       arm_undefined_vector
    b       arm_svc_vector
    b       arm_prefetch_abort_vector
    b       arm_data_abort_vector
    b       arm_reserved_vector
    b       arm_irq_vector
    b       arm_fiq_vector

    .text
    .globl arm_trap_init
    .globl arm_enter_user
    .globl arm_user_fixture
    .extern arm_handle_svc
    .extern arm_handle_irq
    .extern arm_handle_data_abort
    .extern arm_handle_fatal
    .extern arm_timer_fired
    .extern arm_data_abort_seen

arm_trap_init:
    cpsid   if
    cps     #0x12                  /* IRQ */
    ldr     sp, =IRQ_STACK_TOP
    cps     #0x17                  /* abort */
    ldr     sp, =ABT_STACK_TOP
    cps     #0x1b                  /* undefined */
    ldr     sp, =UND_STACK_TOP
    cps     #0x11                  /* FIQ */
    ldr     sp, =FIQ_STACK_TOP
    cps     #0x13                  /* SVC */
    ldr     sp, =SVC_STACK_TOP
    ldr     r0, =arm_vector_table
    mcr     p15, 0, r0, c12, c0, 0
    mrc     p15, 0, r0, c1, c0, 0
    orr     r0, r0, #2            /* SCTLR.A: trap unaligned data accesses */
    mcr     p15, 0, r0, c1, c0, 0
    dsb
    isb
    bx      lr

/* r0=user entry, r1=user stack top */
arm_enter_user:
    cpsid   if
    mov     r2, r0
    mov     r3, r1
    cps     #0x1f                  /* system mode uses the USR register bank */
    mov     sp, r3
    mov     lr, #0
    cps     #0x13
    mov     lr, r2
    mov     r0, #0x50              /* USR mode, FIQ masked, IRQ enabled */
    msr     spsr_cxsf, r0
    movs    pc, lr

.macro save_exception_frame vector
    sub     sp, sp, #FRAME_SIZE
    stmia   sp, {r0-r12}
    str     lr, [sp, #FRAME_PC]
    mrs     r0, spsr
    str     r0, [sp, #FRAME_CPSR]
    add     r0, sp, #FRAME_USR_SP
    stmia   r0, {r13-r14}^
    nop
    mov     r0, #\vector
    str     r0, [sp, #FRAME_VECTOR]
.endm

.macro restore_exception_frame
    add     r0, sp, #FRAME_USR_SP
    ldmia   r0, {r13-r14}^
    nop
    ldr     r0, [sp, #FRAME_CPSR]
    msr     spsr_cxsf, r0
    ldr     lr, [sp, #FRAME_PC]
    ldmia   sp, {r0-r12}
    add     sp, sp, #FRAME_SIZE
    movs    pc, lr
.endm

arm_svc_vector:
    cpsid   i
    save_exception_frame 2
    mov     r0, sp
    bl      arm_handle_svc
    restore_exception_frame

arm_irq_vector:
    sub     lr, lr, #4
    save_exception_frame 6
    mov     r0, sp
    bl      arm_handle_irq
    restore_exception_frame

arm_data_abort_vector:
    cpsid   i
    sub     lr, lr, #4            /* skip the faulting ARM instruction */
    save_exception_frame 4
    mov     r0, sp
    bl      arm_handle_data_abort
    restore_exception_frame

arm_reset_vector:
    mov     r0, #0
    b       arm_fatal_vector

arm_undefined_vector:
    mov     r0, #1
    b       arm_fatal_vector

arm_prefetch_abort_vector:
    sub     lr, lr, #4
    mov     r0, #3
    b       arm_fatal_vector

arm_reserved_vector:
    mov     r0, #5
    b       arm_fatal_vector

arm_fiq_vector:
    sub     lr, lr, #4
    mov     r0, #7

arm_fatal_vector:
    mrs     r1, spsr
    mov     r2, lr
    bl      arm_handle_fatal
1:  wfi
    b       1b

    .section .arm_elf_fixture, "a"
    .align 2
    .globl arm_user_elf_start
    .globl arm_user_elf_end
    .globl arm_user_fixture
    .globl arm_user_fixture_end
    .globl arm_alignment_probe

/* Static ELF32/ARM ET_EXEC containing the process-address-space fixture. */
arm_user_elf_start:
    .byte   0x7f, 'E', 'L', 'F'
    .byte   1, 1, 1, 0
    .space  8
    .short  2                       /* ET_EXEC */
    .short  40                      /* EM_ARM */
    .word   1
    .word   0x00101000              /* e_entry */
    .word   52                      /* e_phoff */
    .word   0
    .word   0
    .short  52
    .short  32
    .short  1
    .short  0
    .short  0
    .short  0

    .word   1                       /* PT_LOAD */
    .word   0                       /* p_offset */
    .word   0x00100000              /* p_vaddr */
    .word   0
    .word   arm_user_fixture_end - arm_user_elf_start
    .word   arm_user_memory_end - arm_user_elf_start
    .word   7                       /* PF_R | PF_W | PF_X */
    .word   4096

    .balign 4096
arm_user_fixture:
    mov     r5, #0
    mov     r6, #0
    movw    r8, #0x3344
    movt    r8, #0x1122
    movw    r9, #0x7788
    movt    r9, #0x5566
    movw    r10, #0xbbcc
    movt    r10, #0x99aa
    movw    r11, #0xff00
    movt    r11, #0xddee
    movw    r12, #0x9bdf
    movt    r12, #0x1357

    adrl    r4, user_bss_probe
    ldr     r4, [r4]
    cmp     r4, #0
    bne     user_frame_failure

    movw    r3, #0x0000
    movt    r3, #0x0040
    ldr     r0, [r3]
    movw    r1, #0xc33c
    movt    r1, #0xa5a5
    cmp     r0, r1
    bne     user_frame_failure
    mvn     r1, r1
    str     r1, [r3]
    ldr     r0, [r3]
    cmp     r0, r1
    bne     user_frame_failure

    mov     r0, #1
    adr     r1, user_svc_message
    mov     r2, #USER_SVC_MESSAGE_SIZE
    mov     r7, #4                /* Linux ARM EABI write */
    svc     #0

    movw    r0, #0x3344
    movt    r0, #0x1122
    cmp     r8, r0
    bne     user_frame_failure
    movw    r0, #0x7788
    movt    r0, #0x5566
    cmp     r9, r0
    bne     user_frame_failure
    movw    r0, #0xbbcc
    movt    r0, #0x99aa
    cmp     r10, r0
    bne     user_frame_failure
    movw    r0, #0xff00
    movt    r0, #0xddee
    cmp     r11, r0
    bne     user_frame_failure
    movw    r0, #0x9bdf
    movt    r0, #0x1357
    cmp     r12, r0
    bne     user_frame_failure

    adr     r4, arm_alignment_probe
    ldr     r6, [r4, #1]          /* SCTLR.A turns this into a data abort */
    cmp     r5, #1                /* abort handler updates the saved frame */
    bne     user_frame_failure

    movw    r4, #0x0000
    movt    r4, #0x4001
    ldr     r4, [r4]              /* supervisor-only kernel section */
    cmp     r4, #1                /* abort handler updates the saved frame */
    bne     user_frame_failure

1:  cmp     r6, #1                /* IRQ handler updates the saved frame */
    bne     1b

    movw    r0, #0x3344
    movt    r0, #0x1122
    cmp     r8, r0
    bne     user_frame_failure
    movw    r0, #0x7788
    movt    r0, #0x5566
    cmp     r9, r0
    bne     user_frame_failure
    movw    r0, #0xbbcc
    movt    r0, #0x99aa
    cmp     r10, r0
    bne     user_frame_failure
    movw    r0, #0xff00
    movt    r0, #0xddee
    cmp     r11, r0
    bne     user_frame_failure
    movw    r0, #0x9bdf
    movt    r0, #0x1357
    cmp     r12, r0
    bne     user_frame_failure

    mov     r0, #1
    adr     r1, user_irq_message
    mov     r2, #USER_IRQ_MESSAGE_SIZE
    mov     r7, #4
    svc     #0

    mov     r0, #0
    mov     r7, #1                /* Linux ARM EABI exit */
    svc     #0

user_frame_failure:
    mov     r0, #2
    mov     r7, #1
    svc     #0
2:  b       2b

    .align 2
arm_alignment_probe:
    .word   0x10203040
user_svc_message:
    .ascii  "Fiwix ARMv7 user SVC passed\n"
user_svc_message_end:
user_irq_message:
    .ascii  "Fiwix ARMv7 timer IRQ passed\n"
user_irq_message_end:
    .equ USER_SVC_MESSAGE_SIZE, user_svc_message_end - user_svc_message
    .equ USER_IRQ_MESSAGE_SIZE, user_irq_message_end - user_irq_message
    .balign 4
arm_user_fixture_end:
    .set user_bss_probe, arm_user_fixture_end
    .set arm_user_memory_end, arm_user_fixture_end + 64
arm_user_elf_end:

    .section .note.GNU-stack,"",%progbits
