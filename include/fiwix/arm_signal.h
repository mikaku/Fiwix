/*
 * fiwix/include/fiwix/arm_signal.h
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_ARM_SIGNAL_H
#define _FIWIX_ARM_SIGNAL_H

#include <fiwix/arm_trap.h>

#define ARM_RT_SIGSET_SIZE	8U
#define ARM_SIGNAL_TRAMPOLINE	ARM_INIT_TRAMPOLINE
#define ARM_USER_STACK_TOP	ARM_SIGNAL_TRAMPOLINE
#define ARM_SA_RESTORER		0x04000000U

struct arm_kernel_sigaction {
	unsigned int handler;
	unsigned int flags;
	unsigned int restorer;
	unsigned int mask[2];
};

struct arm_sigcontext {
	unsigned int trap_no;
	unsigned int error_code;
	unsigned int oldmask;
	unsigned int r[11];
	unsigned int fp;
	unsigned int ip;
	unsigned int sp;
	unsigned int lr;
	unsigned int pc;
	unsigned int cpsr;
	unsigned int fault_address;
};

struct arm_sigaltstack {
	unsigned int sp;
	int flags;
	unsigned int size;
};

struct arm_ucontext {
	unsigned int flags;
	unsigned int link;
	struct arm_sigaltstack stack;
	struct arm_sigcontext mcontext;
	unsigned int sigmask[32];
	unsigned int regspace[128] __attribute__((aligned(8)));
};

struct arm_sigframe {
	struct arm_ucontext context;
	unsigned int retcode[4];
};

struct arm_rt_sigframe {
	unsigned char siginfo[128];
	struct arm_sigframe signal;
};

void arm_signal_frame_build(struct arm_rt_sigframe *,
	const struct arm_trap_frame *, unsigned int, unsigned int);
int arm_signal_frame_restore(struct arm_trap_frame *,
	const struct arm_rt_sigframe *);
int arm_signal_deliver(struct arm_trap_frame *, unsigned int, unsigned int);
int arm_signal_return(struct arm_trap_frame *);
int arm_signal_map(void);
int arm_rt_sigaction(unsigned int, const void *, void *, unsigned int);
int arm_rt_sigprocmask(unsigned int, const void *, void *, unsigned int);

#endif /* _FIWIX_ARM_SIGNAL_H */
