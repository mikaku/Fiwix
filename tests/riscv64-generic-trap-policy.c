#include <fiwix/arch_process.h>
#include <fiwix/kernel.h>
#include <fiwix/riscv64_trap.h>
#include <fiwix/segments.h>
#include <fiwix/sigcontext.h>

#define INTERRUPT_BIT	(1UL << 63)

struct kernel_stat kstat;
int need_resched;

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
	need_resched = 0;
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
		disable_calls != 1 || schedule_calls != 1) {
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
	if(riscv64_generic_user_trap(&frame, 2) != -1 || syscall_calls ||
		bottom_half_calls || schedule_calls) {
		return 4;
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

	return 0;
}
