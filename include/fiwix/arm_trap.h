/*
 * Saved ARMv7 USR state. Keep this layout synchronized with arch/arm/traps.S.
 */

#ifndef _FIWIX_ARM_TRAP_H
#define _FIWIX_ARM_TRAP_H

struct arm_trap_frame {
	unsigned int r[13];
	unsigned int pc;
	unsigned int cpsr;
	unsigned int user_sp;
	unsigned int user_lr;
	unsigned int vector;
};

int arm_eabi_user_syscall(struct arm_trap_frame *);

#endif /* _FIWIX_ARM_TRAP_H */
