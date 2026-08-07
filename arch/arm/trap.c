/*
 * fiwix/arch/arm/trap.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_trap.h>
#include <fiwix/asm.h>
#include <fiwix/irq.h>
#include <fiwix/kernel.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/sched.h>
#include <fiwix/segments.h>
#include <fiwix/signal.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/timer.h>

#define ARM_FAULT_STATUS_MASK	0x40FU
#define ARM_DFSR_WRITE		0x800U

#define ARM_FAULT_ALIGNMENT	0x001U
#define ARM_FAULT_ACCESS_SECTION 0x003U
#define ARM_FAULT_TRANSLATION_SECTION 0x005U
#define ARM_FAULT_ACCESS_PAGE	0x006U
#define ARM_FAULT_TRANSLATION_PAGE 0x007U
#define ARM_FAULT_PERMISSION_SECTION 0x00DU
#define ARM_FAULT_PERMISSION_PAGE 0x00FU

static int arm_is_page_fault(unsigned int status)
{
	switch(status & ARM_FAULT_STATUS_MASK) {
		case ARM_FAULT_ACCESS_SECTION:
		case ARM_FAULT_TRANSLATION_SECTION:
		case ARM_FAULT_ACCESS_PAGE:
		case ARM_FAULT_TRANSLATION_PAGE:
		case ARM_FAULT_PERMISSION_SECTION:
		case ARM_FAULT_PERMISSION_PAGE:
			return 1;
	}
	return 0;
}

static void arm_run_bottom_halves(int from_user)
{
	struct sigcontext compat;

	memset_b(&compat, 0, sizeof(compat));
	compat.cs = from_user ? USER_CS : KERNEL_CS;
	arm_interrupt_enable();
	do_bh(compat);
	arm_interrupt_disable();
}

static void arm_user_return(struct arm_trap_frame *frame)
{
	arm_run_bottom_halves(1);
	if(issig()) {
		psig((__addr_t)(unsigned long)frame);
	}
	if(need_resched) {
		do_sched();
	}
}

static int arm_page_fault(struct arm_trap_frame *frame,
	unsigned int address, unsigned int status, int from_user,
	int write)
{
	__addr_t user_sp;
	unsigned int flags;
	int result;

	if(!current || !arm_is_page_fault(status)) {
		return -1;
	}
	flags = from_user ? PFAULT_U : 0;
	if(write) {
		flags |= PFAULT_W;
	}
	if(get_mapped_addr(current, address) & PAGE_PRESENT) {
		flags |= PFAULT_V;
	}
	user_sp = from_user ? frame->user_sp : 0;
	if(!from_user && current->sp) {
		user_sp = ((struct arm_trap_frame *)
			(unsigned long)current->sp)->user_sp;
	}
	result = resolve_page_fault(address, flags, user_sp);
	if(result == PFAULT_RESOLVED) {
		return 0;
	}
	if(result == PFAULT_SIGSEGV) {
		send_sig(current, SIGSEGV);
		return 0;
	}
	if(result == PFAULT_SIGKILL) {
		printk("ARM page fault allocation failed: pid=%d address=%x "
			"rss=%u free=%d buffers=%dKB\n", current->pid,
			address, current->rss, kstat.free_pages,
			kstat.buffers_size);
		send_sig(current, SIGKILL);
		return 0;
	}
	return -1;
}

static int arm_abort(struct arm_trap_frame *frame, int from_user)
{
	unsigned int address;
	unsigned int status;
	int write;

	if(frame->vector == ARM_TRAP_PREFETCH_ABORT) {
		address = arm_read_ifar();
		status = arm_read_ifsr();
		write = 0;
	} else {
		address = arm_read_dfar();
		status = arm_read_dfsr();
		write = (status & ARM_DFSR_WRITE) != 0;
	}
	if(arm_is_page_fault(status)) {
		return arm_page_fault(frame, address, status, from_user, write);
	}
	if(from_user) {
		send_sig(current,
			(status & ARM_FAULT_STATUS_MASK) == ARM_FAULT_ALIGNMENT ?
			SIGBUS : SIGSEGV);
		return 0;
	}
	return -1;
}

static int arm_irq(int from_user)
{
	struct sigcontext compat;
	unsigned int interrupt;
	unsigned int token;

	token = arm_generic_irq_claim();
	interrupt = token & 0x3FFU;
	if(interrupt >= ARM_GIC_SPURIOUS_BASE) {
		return 0;
	}
	if(interrupt != ARM_PHYS_TIMER_IRQ) {
		arm_generic_irq_complete(token);
		return -1;
	}
	arm_generic_timer_rearm();
	memset_b(&compat, 0, sizeof(compat));
	compat.cs = from_user ? USER_CS : KERNEL_CS;
	irq_timer(TIMER_IRQ, &compat);
	arm_generic_irq_complete(token);
	return 0;
}

int arm_generic_user_trap(struct arm_trap_frame *frame)
{
	if(!current) {
		return -1;
	}
	current->sp = (__addr_t)(unsigned long)frame;
	switch(frame->vector) {
		case ARM_TRAP_SVC:
			if(arm_eabi_user_syscall(frame)) {
				return -1;
			}
			break;
		case ARM_TRAP_PREFETCH_ABORT:
		case ARM_TRAP_DATA_ABORT:
			if(arm_abort(frame, 1)) {
				return -1;
			}
			break;
		case ARM_TRAP_UNDEFINED:
			send_sig(current, SIGILL);
			break;
		case ARM_TRAP_IRQ:
			if(arm_irq(1)) {
				return -1;
			}
			break;
		default:
			return -1;
	}
	arm_user_return(frame);
	return 0;
}

int arm_generic_kernel_trap(struct arm_trap_frame *frame)
{
	switch(frame->vector) {
		case ARM_TRAP_PREFETCH_ABORT:
		case ARM_TRAP_DATA_ABORT:
			if(arm_abort(frame, 0)) {
				return -1;
			}
			break;
		case ARM_TRAP_IRQ:
			if(arm_irq(0)) {
				return -1;
			}
			break;
		default:
			return -1;
	}
	arm_run_bottom_halves(0);
	return 0;
}

void arm_generic_trap_fatal(struct arm_trap_frame *frame)
{
	PANIC("ARM trap: vector=%u pc=%x cpsr=%x\n",
		frame->vector, frame->pc, frame->cpsr);
}
