#include <fiwix/errno.h>
#include <fiwix/process.h>
#include <fiwix/riscv64_signal.h>
#include <fiwix/sched.h>
#include <fiwix/signal.h>
#include <fiwix/syscalls.h>

struct proc process;
struct proc parent;
struct proc *current = &process;
int need_resched;

static int deliver_calls;
static unsigned int delivered_signal;
static unsigned int delivered_oldmask;
static struct riscv64_trap_frame *delivered_frame;
static int exit_calls;
static int exit_signal;

static void test_handler(int signum)
{
	(void)signum;
}

int riscv64_signal_deliver(struct riscv64_trap_frame *frame,
	unsigned int signum, unsigned int oldmask)
{
	deliver_calls++;
	delivered_frame = frame;
	delivered_signal = signum;
	delivered_oldmask = oldmask;
	return 0;
}

void do_exit(int signum)
{
	exit_calls++;
	exit_signal = signum;
}

void runnable(struct proc *p)
{
	(void)p;
}

void not_runnable(struct proc *p, int state)
{
	(void)p;
	(void)state;
}

void wakeup_proc(struct proc *p)
{
	(void)p;
}

void wakeup(void *address)
{
	(void)address;
}

int sys_wait4(int pid, int *status, int options, struct rusage *usage)
{
	(void)pid;
	(void)status;
	(void)options;
	(void)usage;
	return 0;
}

int sys_waitpid(__pid_t pid, int *status, int options)
{
	(void)pid;
	(void)status;
	(void)options;
	return 0;
}

struct proc *get_next_zombie(struct proc *p)
{
	(void)p;
	return 0;
}

__pid_t remove_zombie(struct proc *p)
{
	(void)p;
	return 0;
}

void printk(const char *format, ...)
{
	(void)format;
}

static void clear_process(void)
{
	unsigned char *byte;
	unsigned int count;

	byte = (unsigned char *)&process;
	count = sizeof(process);
	while(count--) {
		*byte++ = 0;
	}
	process.pid = 2;
	process.ppid = &parent;
	deliver_calls = 0;
	delivered_signal = 0;
	delivered_oldmask = 0;
	delivered_frame = 0;
	exit_calls = 0;
	exit_signal = 0;
	need_resched = 0;
}

int main(void)
{
	struct riscv64_trap_frame frame;
	unsigned int usr1, usr2, alarm;

	usr1 = 1U << (SIGUSR1 - 1);
	usr2 = 1U << (SIGUSR2 - 1);
	alarm = 1U << (SIGALRM - 1);
	clear_process();
	process.sigpending = usr1;
	process.sigblocked = alarm;
	process.sigaction[SIGUSR1 - 1].sa_handler = test_handler;
	process.sigaction[SIGUSR1 - 1].sa_mask = usr2;
	process.sigaction[SIGUSR1 - 1].sa_flags = SA_RESETHAND;
	psig((__addr_t)&frame);
	if(deliver_calls != 1 || delivered_frame != &frame ||
		delivered_signal != SIGUSR1 || delivered_oldmask != alarm ||
		process.sigpending || process.sigexecuting != usr1 ||
		process.sigblocked != (alarm | usr1 | usr2) ||
		process.sigaction[SIGUSR1 - 1].sa_handler != SIG_DFL ||
		exit_calls) {
		return 1;
	}

	clear_process();
	process.sigpending = 1U << (SIGTERM - 1);
	psig((__addr_t)&frame);
	if(exit_calls != 1 || exit_signal != SIGTERM || deliver_calls) {
		return 2;
	}

	clear_process();
	if(send_sig(&process, NSIG) != -EINVAL || process.sigpending) {
		return 3;
	}
	if((SIG_BLOCKABLE & (1U << (SIGKILL - 1))) ||
		(SIG_BLOCKABLE & (1U << (SIGSTOP - 1))) ||
		!(SIG_BLOCKABLE & usr1)) {
		return 4;
	}

	return 0;
}
