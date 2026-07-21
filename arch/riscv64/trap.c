/* Generic Fiwix trap policy for the RV64 entry in generic-trap.S. */

#include <fiwix/arch_process.h>
#include <fiwix/asm.h>
#include <fiwix/irq.h>
#include <fiwix/kernel.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/riscv64_trap.h>
#include <fiwix/sched.h>
#include <fiwix/segments.h>
#include <fiwix/signal.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/timer.h>

#define RV_SCAUSE_INTERRUPT	(1UL << 63)
#define RV_SCAUSE_CODE		(~RV_SCAUSE_INTERRUPT)
#define RV_SCAUSE_SSIP		1UL
#define RV_SCAUSE_U_ECALL	8UL
#define RV_SCAUSE_INST_PAGE	12UL
#define RV_SCAUSE_LOAD_PAGE	13UL
#define RV_SCAUSE_STORE_PAGE	15UL

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

static int riscv64_page_fault(struct riscv64_trap_frame *frame,
	unsigned long cause, int from_user)
{
	__addr_t user_sp;
	unsigned int flags;
	int result;

	if(!current) {
		return -1;
	}
	flags = from_user ? PFAULT_U : 0;
	if(cause == RV_SCAUSE_STORE_PAGE) {
		flags |= PFAULT_W;
	}
	if(get_mapped_addr(current, frame->stval) & PAGE_PRESENT) {
		flags |= PFAULT_V;
	}
	user_sp = from_user ? frame->sp : 0;
	if(!from_user && current->sp) {
		user_sp = ((struct riscv64_trap_frame *)current->sp)->sp;
	}
	result = resolve_page_fault(frame->stval, flags, user_sp);
	if(result == PFAULT_RESOLVED) {
		return 0;
	}
	if(result == PFAULT_SIGSEGV) {
		send_sig(current, SIGSEGV);
	} else if(result == PFAULT_SIGKILL) {
		send_sig(current, SIGKILL);
	}
	return -1;
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
		if(cause == RV_SCAUSE_U_ECALL) {
			if(riscv64_user_syscall(frame, cause)) {
				return -1;
			}
		} else if(cause == RV_SCAUSE_INST_PAGE ||
			cause == RV_SCAUSE_LOAD_PAGE ||
			cause == RV_SCAUSE_STORE_PAGE) {
			if(riscv64_page_fault(frame, cause, 1)) {
				return -1;
			}
		} else {
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
	if((cause & RV_SCAUSE_INTERRUPT) &&
		(cause & RV_SCAUSE_CODE) == RV_SCAUSE_SSIP) {
		return riscv64_timer_trap(0);
	}
	if(cause == RV_SCAUSE_INST_PAGE || cause == RV_SCAUSE_LOAD_PAGE ||
		cause == RV_SCAUSE_STORE_PAGE) {
		struct riscv64_trap_frame frame;
		int result;

		memset_b(&frame, 0, sizeof(frame));
		frame.stval = value;
		result = riscv64_page_fault(&frame, cause, 0);
		if(!result) {
			riscv64_run_bottom_halves(0);
		}
		return result;
	}
	return -1;
}

void riscv64_generic_trap_fatal(unsigned long cause, unsigned long epc,
	unsigned long value)
{
	PANIC("RV64 trap: scause=%lx sepc=%lx stval=%lx\n",
		cause, epc, value);
}
