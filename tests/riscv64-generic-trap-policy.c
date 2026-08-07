#include <fiwix/arch_process.h>
#include <fiwix/kernel.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/riscv64_trap.h>
#include <fiwix/segments.h>
#include <fiwix/signal.h>
#include <fiwix/sigcontext.h>

#define INTERRUPT_BIT	(1UL << 63)

struct kernel_stat kstat;
int need_resched;
struct proc *current;

static int clear_calls;
static int enable_calls;
static int disable_calls;
static int timer_calls;
static int bottom_half_calls;
static int schedule_calls;
static int syscall_calls;
static int syscall_result;
static int last_timer_cs;
static int last_bottom_half_cs;
static struct riscv64_trap_frame *last_frame;
static unsigned long last_cause;
static __addr_t mapped_address;
static __addr_t fault_address;
static __addr_t fault_user_sp;
static unsigned int fault_flags;
static int fault_calls;
static int fault_result;
static int signal_calls;
static int last_signal;
static int pending_signal;
static int psig_calls;
static __addr_t psig_stack;
static struct proc test_process;
static struct riscv64_trap_frame saved_user_frame;

void memset_b(void *destination, unsigned char value, unsigned int count)
{
	unsigned char *byte;

	byte = (unsigned char *)destination;
	while(count--) {
		*byte++ = value;
	}
}

void riscv64_clear_ssip(void)
{
	clear_calls++;
}

void riscv64_interrupt_enable(void)
{
	enable_calls++;
}

void riscv64_interrupt_disable(void)
{
	disable_calls++;
}

void irq_timer(int num, struct sigcontext *context)
{
	if(num) {
		timer_calls = -100;
		return;
	}
	timer_calls++;
	last_timer_cs = context->cs;
}

void do_bh(struct sigcontext context)
{
	bottom_half_calls++;
	last_bottom_half_cs = context.cs;
}

void do_sched(void)
{
	schedule_calls++;
}

int riscv64_user_syscall(struct riscv64_trap_frame *frame,
	unsigned long cause)
{
	syscall_calls++;
	last_frame = frame;
	last_cause = cause;
	return syscall_result;
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

void psig(__addr_t stack)
{
	psig_calls++;
	psig_stack = stack;
	pending_signal = 0;
}

void printk(const char *format, ...)
{
	(void)format;
}

void stop_kernel(void)
{
}

static void clear_counts(void)
{
	clear_calls = 0;
	enable_calls = 0;
	disable_calls = 0;
	timer_calls = 0;
	bottom_half_calls = 0;
	schedule_calls = 0;
	syscall_calls = 0;
	syscall_result = 0;
	last_timer_cs = 0;
	last_bottom_half_cs = 0;
	last_frame = 0;
	last_cause = 0;
	mapped_address = 0;
	fault_address = 0;
	fault_user_sp = 0;
	fault_flags = 0;
	fault_calls = 0;
	fault_result = PFAULT_RESOLVED;
	signal_calls = 0;
	last_signal = 0;
	pending_signal = 0;
	psig_calls = 0;
	psig_stack = 0;
	need_resched = 0;
	current = &test_process;
	current->sp = (__addr_t)&saved_user_frame;
}

int main(void)
{
	struct riscv64_trap_frame frame;

	clear_counts();
	need_resched = 1;
	if(riscv64_generic_user_trap(&frame, 8) || syscall_calls != 1 ||
		last_frame != &frame || last_cause != 8 || clear_calls ||
		timer_calls || bottom_half_calls != 1 ||
		last_bottom_half_cs != USER_CS || enable_calls != 1 ||
		disable_calls != 1 || schedule_calls != 1 ||
		current->sp != (__addr_t)&frame) {
		return 1;
	}

	clear_counts();
	need_resched = 1;
	if(riscv64_generic_user_trap(&frame, INTERRUPT_BIT | 1) ||
		clear_calls != 1 || timer_calls != 1 || last_timer_cs != USER_CS ||
		bottom_half_calls != 1 || last_bottom_half_cs != USER_CS ||
		schedule_calls != 1 || syscall_calls) {
		return 2;
	}

	clear_counts();
	need_resched = 1;
	if(riscv64_generic_kernel_trap(INTERRUPT_BIT | 1, 2, 3) ||
		clear_calls != 1 || timer_calls != 1 ||
		last_timer_cs != KERNEL_CS || bottom_half_calls != 1 ||
		last_bottom_half_cs != KERNEL_CS || schedule_calls) {
		return 3;
	}

	clear_counts();
	if(riscv64_generic_user_trap(&frame, 2) || syscall_calls ||
		bottom_half_calls != 1 || schedule_calls || signal_calls != 1 ||
		last_signal != SIGILL || psig_calls != 1) {
		return 4;
	}
	clear_counts();
	if(riscv64_generic_user_trap(&frame, 10) != -1 || syscall_calls ||
		bottom_half_calls || signal_calls || psig_calls) {
		return 13;
	}
	if(riscv64_generic_kernel_trap(INTERRUPT_BIT | 9, 2, 3) != -1) {
		return 5;
	}

	clear_counts();
	syscall_result = -1;
	if(riscv64_generic_user_trap(&frame, 8) != -1 || syscall_calls != 1 ||
		bottom_half_calls || schedule_calls) {
		return 6;
	}

	clear_counts();
	frame.sp = 0x3000;
	frame.stval = 0x4000;
	if(riscv64_generic_user_trap(&frame, 13) || fault_calls != 1 ||
		fault_address != 0x4000 || fault_flags != PFAULT_U ||
		fault_user_sp != 0x3000 || bottom_half_calls != 1) {
		return 7;
	}

	clear_counts();
	frame.sp = 0x5000;
	frame.stval = mapped_address = 0x6000;
	if(riscv64_generic_user_trap(&frame, 15) || fault_calls != 1 ||
		fault_flags != (PFAULT_U | PFAULT_V | PFAULT_W) ||
		fault_user_sp != 0x5000) {
		return 8;
	}

	clear_counts();
	frame.stval = 0x7000;
	fault_result = PFAULT_SIGSEGV;
	if(riscv64_generic_user_trap(&frame, 13) ||
		signal_calls != 1 || last_signal != SIGSEGV ||
		bottom_half_calls != 1 || psig_calls != 1 ||
		psig_stack != (__addr_t)&frame) {
		return 9;
	}

	clear_counts();
	saved_user_frame.sp = 0x8000;
	if(riscv64_generic_kernel_trap(13, 0x9000, 0xa000) ||
		fault_calls != 1 || fault_address != 0xa000 || fault_flags ||
		fault_user_sp != 0x8000 || bottom_half_calls != 1 ||
		last_bottom_half_cs != KERNEL_CS || schedule_calls) {
		return 10;
	}

	clear_counts();
	current = 0;
	if(riscv64_generic_kernel_trap(13, 0x9000, 0xa000) != -1 ||
		fault_calls || signal_calls) {
		return 11;
	}

	clear_counts();
	current = 0;
	if(riscv64_generic_user_trap(&frame, 8) != -1 || syscall_calls ||
		bottom_half_calls || psig_calls) {
		return 12;
	}

	return 0;
}
