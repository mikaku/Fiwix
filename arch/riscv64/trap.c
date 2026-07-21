/* Generic Fiwix trap policy for the RV64 entry in generic-trap.S. */

#include <fiwix/arch_process.h>
#include <fiwix/asm.h>
#include <fiwix/irq.h>
#include <fiwix/kernel.h>
#include <fiwix/riscv64_trap.h>
#include <fiwix/sched.h>
#include <fiwix/segments.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/timer.h>

#define RV_SCAUSE_INTERRUPT	(1UL << 63)
#define RV_SCAUSE_CODE		(~RV_SCAUSE_INTERRUPT)
#define RV_SCAUSE_SSIP		1UL
#define RV_SCAUSE_U_ECALL	8UL

static void riscv64_run_bottom_halves(int from_user)
{
	struct sigcontext compat;

	memset_b(&compat, 0, sizeof(compat));
	compat.cs = from_user ? USER_CS : KERNEL_CS;
	riscv64_interrupt_enable();
	do_bh(compat);
	riscv64_interrupt_disable();
}

static int riscv64_timer_trap(int from_user)
{
	struct sigcontext compat;

	riscv64_clear_ssip();
	memset_b(&compat, 0, sizeof(compat));
	compat.cs = from_user ? USER_CS : KERNEL_CS;
	irq_timer(TIMER_IRQ, &compat);
	riscv64_run_bottom_halves(from_user);
	return 0;
}

int riscv64_generic_user_trap(struct riscv64_trap_frame *frame,
	unsigned long cause)
{
	if(cause & RV_SCAUSE_INTERRUPT) {
		if((cause & RV_SCAUSE_CODE) != RV_SCAUSE_SSIP) {
			return -1;
		}
		riscv64_timer_trap(1);
	} else {
		if(cause != RV_SCAUSE_U_ECALL ||
			riscv64_user_syscall(frame, cause)) {
			return -1;
		}
		riscv64_run_bottom_halves(1);
	}
	if(need_resched) {
		do_sched();
	}
	return 0;
}

int riscv64_generic_kernel_trap(unsigned long cause, unsigned long epc,
	unsigned long value)
{
	(void)epc;
	(void)value;
	if((cause & RV_SCAUSE_INTERRUPT) &&
		(cause & RV_SCAUSE_CODE) == RV_SCAUSE_SSIP) {
		return riscv64_timer_trap(0);
	}
	return -1;
}

void riscv64_generic_trap_fatal(unsigned long cause, unsigned long epc,
	unsigned long value)
{
	PANIC("RV64 trap: scause=%lx sepc=%lx stval=%lx\n",
		cause, epc, value);
}
