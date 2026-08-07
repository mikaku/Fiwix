/*
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_trap.h>
#include <fiwix/kernel.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/segments.h>
#include <fiwix/signal.h>
#include <fiwix/sigcontext.h>

struct kernel_stat kstat;
int need_resched;
struct proc *current;

static struct proc test_process;
static struct arm_trap_frame saved_user_frame;
static int syscall_calls;
static int syscall_result;
static int enable_calls;
static int disable_calls;
static int bottom_half_calls;
static int bottom_half_cs;
static int schedule_calls;
static int signal_calls;
static int last_signal;
static int pending_signal;
static int psig_calls;
static __addr_t psig_frame;
static unsigned int dfsr;
static unsigned int dfar;
static unsigned int ifsr;
static unsigned int ifar;
static __addr_t mapped_address;
static int fault_calls;
static __addr_t fault_address;
static __addr_t fault_user_sp;
static unsigned int fault_flags;
static int fault_result;
static unsigned int irq_token;
static int irq_claim_calls;
static int irq_complete_calls;
static unsigned int completed_token;
static int timer_rearm_calls;
static int timer_calls;
static int timer_cs;
static int irq_sequence;

void memset_b(void *destination, unsigned char value, unsigned int count)
{
	unsigned char *byte;

	byte = (unsigned char *)destination;
	while(count--) {
		*byte++ = value;
	}
}

void arm_interrupt_enable(void)
{
	enable_calls++;
}

void arm_interrupt_disable(void)
{
	disable_calls++;
}

void do_bh(struct sigcontext context)
{
	bottom_half_calls++;
	bottom_half_cs = context.cs;
}

void do_sched(void)
{
	schedule_calls++;
}

int arm_eabi_user_syscall(struct arm_trap_frame *frame)
{
	(void)frame;
	syscall_calls++;
	return syscall_result;
}

unsigned int arm_read_dfsr(void)
{
	return dfsr;
}

unsigned int arm_read_dfar(void)
{
	return dfar;
}

unsigned int arm_read_ifsr(void)
{
	return ifsr;
}

unsigned int arm_read_ifar(void)
{
	return ifar;
}

__addr_t get_mapped_addr(struct proc *process, __addr_t address)
{
	if(process != current || address != mapped_address) {
		return 0;
	}
	return address | PAGE_PRESENT;
}

int resolve_page_fault(__addr_t address, unsigned int flags,
	__addr_t user_sp)
{
	fault_calls++;
	fault_address = address;
	fault_flags = flags;
	fault_user_sp = user_sp;
	return fault_result;
}

int send_sig(struct proc *process, __sigset_t signal)
{
	if(process != current) {
		return -1;
	}
	signal_calls++;
	last_signal = signal;
	pending_signal = signal;
	return 0;
}

int issig(void)
{
	return pending_signal;
}

void psig(__addr_t frame)
{
	psig_calls++;
	psig_frame = frame;
	pending_signal = 0;
}

unsigned int arm_generic_irq_claim(void)
{
	irq_claim_calls++;
	return irq_token;
}

void arm_generic_irq_complete(unsigned int token)
{
	irq_complete_calls++;
	completed_token = token;
	irq_sequence = irq_sequence * 10 + 3;
}

void arm_generic_timer_rearm(void)
{
	timer_rearm_calls++;
	irq_sequence = irq_sequence * 10 + 1;
}

void irq_timer(int num, struct sigcontext *context)
{
	if(num) {
		timer_calls = -100;
		return;
	}
	timer_calls++;
	timer_cs = context->cs;
	irq_sequence = irq_sequence * 10 + 2;
}

void printk(const char *format, ...)
{
	(void)format;
}

void stop_kernel(void)
{
}

static void reset(void)
{
	memset_b(&test_process, 0, sizeof(test_process));
	memset_b(&saved_user_frame, 0, sizeof(saved_user_frame));
	syscall_calls = 0;
	syscall_result = 0;
	enable_calls = 0;
	disable_calls = 0;
	bottom_half_calls = 0;
	bottom_half_cs = 0;
	schedule_calls = 0;
	signal_calls = 0;
	last_signal = 0;
	pending_signal = 0;
	psig_calls = 0;
	psig_frame = 0;
	dfsr = 0;
	dfar = 0;
	ifsr = 0;
	ifar = 0;
	mapped_address = 0;
	fault_calls = 0;
	fault_address = 0;
	fault_user_sp = 0;
	fault_flags = 0;
	fault_result = PFAULT_RESOLVED;
	irq_token = ARM_PHYS_TIMER_IRQ;
	irq_claim_calls = 0;
	irq_complete_calls = 0;
	completed_token = 0;
	timer_rearm_calls = 0;
	timer_calls = 0;
	timer_cs = 0;
	irq_sequence = 0;
	need_resched = 0;
	current = &test_process;
	saved_user_frame.user_sp = 0x8000;
	current->sp = (__addr_t)(unsigned long)&saved_user_frame;
}

int main(void)
{
	struct arm_trap_frame frame;

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_SVC;
	need_resched = 1;
	if(arm_generic_user_trap(&frame) || syscall_calls != 1 ||
		current->sp != (__addr_t)(unsigned long)&frame ||
		bottom_half_calls != 1 ||
		bottom_half_cs != USER_CS || enable_calls != 1 ||
		disable_calls != 1 || schedule_calls != 1) {
		return 1;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_SVC;
	syscall_result = -1;
	if(arm_generic_user_trap(&frame) != -1 || syscall_calls != 1 ||
		bottom_half_calls || schedule_calls) {
		return 2;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_UNDEFINED;
	if(arm_generic_user_trap(&frame) || signal_calls != 1 ||
		last_signal != SIGILL || psig_calls != 1 ||
		psig_frame != (__addr_t)(unsigned long)&frame ||
		bottom_half_calls != 1) {
		return 3;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_PREFETCH_ABORT;
	frame.user_sp = 0x3000;
	ifsr = 5;
	ifar = 0x4000;
	if(arm_generic_user_trap(&frame) || fault_calls != 1 ||
		fault_address != 0x4000 || fault_flags != PFAULT_U ||
		fault_user_sp != 0x3000 || signal_calls) {
		return 4;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_DATA_ABORT;
	frame.user_sp = 0x5000;
	dfsr = 0x80F;
	dfar = mapped_address = 0x6000;
	if(arm_generic_user_trap(&frame) || fault_calls != 1 ||
		fault_flags != (PFAULT_U | PFAULT_V | PFAULT_W) ||
		fault_user_sp != 0x5000) {
		return 5;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_DATA_ABORT;
	dfsr = 1;
	dfar = 0x7000;
	if(arm_generic_user_trap(&frame) || fault_calls ||
		signal_calls != 1 || last_signal != SIGBUS || psig_calls != 1) {
		return 6;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_DATA_ABORT;
	frame.user_sp = 0x7100;
	dfsr = 7;
	dfar = 0x7200;
	fault_result = PFAULT_SIGSEGV;
	if(arm_generic_user_trap(&frame) || fault_calls != 1 ||
		signal_calls != 1 || last_signal != SIGSEGV ||
		psig_calls != 1 ||
		psig_frame != (__addr_t)(unsigned long)&frame) {
		return 7;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_IRQ;
	irq_token = 0x40000000U | ARM_PHYS_TIMER_IRQ;
	need_resched = 1;
	if(arm_generic_user_trap(&frame) || irq_claim_calls != 1 ||
		timer_rearm_calls != 1 || timer_calls != 1 ||
		timer_cs != USER_CS || irq_complete_calls != 1 ||
		completed_token != irq_token || irq_sequence != 123 ||
		bottom_half_cs != USER_CS || schedule_calls != 1) {
		return 8;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_IRQ;
	if(arm_generic_kernel_trap(&frame) || timer_calls != 1 ||
		timer_cs != KERNEL_CS || irq_sequence != 123 ||
		bottom_half_calls != 1 || bottom_half_cs != KERNEL_CS ||
		schedule_calls) {
		return 9;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_IRQ;
	irq_token = 31;
	if(arm_generic_user_trap(&frame) != -1 || timer_calls ||
		irq_complete_calls != 1 || completed_token != 31 ||
		bottom_half_calls) {
		return 10;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_IRQ;
	irq_token = 1023;
	if(arm_generic_user_trap(&frame) || timer_calls ||
		irq_complete_calls || bottom_half_calls != 1) {
		return 11;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_DATA_ABORT;
	dfsr = 5;
	dfar = 0xA000;
	if(arm_generic_kernel_trap(&frame) || fault_calls != 1 ||
		fault_address != 0xA000 || fault_flags ||
		fault_user_sp != 0x8000 || bottom_half_calls != 1 ||
		bottom_half_cs != KERNEL_CS) {
		return 12;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_PREFETCH_ABORT;
	ifsr = 2;
	ifar = 0xB000;
	if(arm_generic_user_trap(&frame) || fault_calls ||
		signal_calls != 1 || last_signal != SIGSEGV) {
		return 13;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_SVC;
	current = 0;
	if(arm_generic_user_trap(&frame) != -1 || syscall_calls ||
		bottom_half_calls) {
		return 14;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_DATA_ABORT;
	dfsr = 5;
	current = 0;
	if(arm_generic_kernel_trap(&frame) != -1 || fault_calls ||
		signal_calls) {
		return 15;
	}

	reset();
	memset_b(&frame, 0, sizeof(frame));
	frame.vector = ARM_TRAP_FIQ;
	if(arm_generic_user_trap(&frame) != -1 || bottom_half_calls ||
		signal_calls) {
		return 16;
	}

	return 0;
}
