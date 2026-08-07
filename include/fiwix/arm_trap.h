/*
 * fiwix/include/fiwix/arm_trap.h
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_ARM_TRAP_H
#define _FIWIX_ARM_TRAP_H

#define ARM_INIT_TRAMPOLINE	0x3FFFF000U
#define ARM_INIT_STACK		0x3FFFE000U

#define ARM_TRAP_UNDEFINED	1U
#define ARM_TRAP_SVC		2U
#define ARM_TRAP_PREFETCH_ABORT	3U
#define ARM_TRAP_DATA_ABORT	4U
#define ARM_TRAP_IRQ		6U
#define ARM_TRAP_FIQ		7U

#define ARM_PHYS_TIMER_IRQ	30U
#define ARM_GIC_SPURIOUS_BASE	1020U

struct arm_trap_frame {
	unsigned int r[13];
	unsigned int pc;
	unsigned int cpsr;
	unsigned int user_sp;
	unsigned int user_lr;
	unsigned int vector;
};

int arm_eabi_user_syscall(struct arm_trap_frame *);
int arm_generic_user_trap(struct arm_trap_frame *);
int arm_generic_kernel_trap(struct arm_trap_frame *);
void arm_generic_trap_fatal(struct arm_trap_frame *);

unsigned int arm_generic_irq_claim(void);
void arm_generic_irq_complete(unsigned int);
void arm_generic_timer_rearm(void);

#endif /* _FIWIX_ARM_TRAP_H */
