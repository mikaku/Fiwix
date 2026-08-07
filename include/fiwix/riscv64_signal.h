/*
 * fiwix/include/fiwix/riscv64_signal.h
 *
 * Linux-compatible RV64 real-time signal ABI used at the user boundary.
 */

#ifndef _FIWIX_RISCV64_SIGNAL_H
#define _FIWIX_RISCV64_SIGNAL_H

#include <fiwix/riscv64_trap.h>

#define RISCV64_RT_SIGSET_SIZE		8UL
#define RISCV64_SIGNAL_TRAMPOLINE	0x0000003ffffff000UL
#define RISCV64_USER_STACK_TOP		RISCV64_SIGNAL_TRAMPOLINE

struct riscv64_kernel_sigaction {
	unsigned long handler;
	unsigned long flags;
	unsigned long mask;
};

struct riscv64_user_regs {
	unsigned long pc;
	unsigned long ra;
	unsigned long sp;
	unsigned long gp;
	unsigned long tp;
	unsigned long t0;
	unsigned long t1;
	unsigned long t2;
	unsigned long s0;
	unsigned long s1;
	unsigned long a0;
	unsigned long a1;
	unsigned long a2;
	unsigned long a3;
	unsigned long a4;
	unsigned long a5;
	unsigned long a6;
	unsigned long a7;
	unsigned long s2;
	unsigned long s3;
	unsigned long s4;
	unsigned long s5;
	unsigned long s6;
	unsigned long s7;
	unsigned long s8;
	unsigned long s9;
	unsigned long s10;
	unsigned long s11;
	unsigned long t3;
	unsigned long t4;
	unsigned long t5;
	unsigned long t6;
};

struct riscv64_fp_state {
	unsigned long words[66];
} __attribute__((aligned(16)));

struct riscv64_sigcontext {
	struct riscv64_user_regs regs;
	struct riscv64_fp_state fp;
};

struct riscv64_sigaltstack {
	unsigned long sp;
	int flags;
	unsigned int padding;
	unsigned long size;
};

struct riscv64_ucontext {
	unsigned long flags;
	unsigned long link;
	struct riscv64_sigaltstack stack;
	unsigned long sigmask;
	unsigned char unused[120];
	struct riscv64_sigcontext mcontext;
};

struct riscv64_rt_sigframe {
	unsigned char siginfo[128];
	struct riscv64_ucontext context;
};

struct proc;

void riscv64_signal_frame_build(struct riscv64_rt_sigframe *,
	const struct riscv64_trap_frame *, unsigned int, unsigned int);
int riscv64_signal_frame_restore(struct riscv64_trap_frame *,
	const struct riscv64_rt_sigframe *);
int riscv64_signal_deliver(struct riscv64_trap_frame *, unsigned int,
	unsigned int);
int riscv64_signal_return(struct riscv64_trap_frame *);
int riscv64_signal_map(void);
int riscv64_rt_sigaction(unsigned long, const void *, void *, unsigned long);
int riscv64_rt_sigprocmask(unsigned long, const void *, void *, unsigned long);

#endif /* _FIWIX_RISCV64_SIGNAL_H */
