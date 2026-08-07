/* Linux RV64 signal-frame and signal-syscall translation. */

#include <fiwix/asm.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/process.h>
#include <fiwix/riscv64_signal.h>
#include <fiwix/signal.h>
#include <fiwix/string.h>
#include <fiwix/syscalls.h>

#define RISCV64_ALIGN16_MASK	0xFFFFFFFFFFFFFFF0UL

#define RV_SSTATUS_SIE		0x00000002UL
#define RV_SSTATUS_SPIE		0x00000020UL
#define RV_SSTATUS_SPP		0x00000100UL

typedef char riscv64_user_regs_size_must_be_256[
	(sizeof(struct riscv64_user_regs) == 256) ? 1 : -1];
typedef char riscv64_kernel_sigaction_size_must_be_24[
	(sizeof(struct riscv64_kernel_sigaction) == 24) ? 1 : -1];
typedef char riscv64_sigcontext_size_must_be_784[
	(sizeof(struct riscv64_sigcontext) == 784) ? 1 : -1];
typedef char riscv64_ucontext_size_must_be_960[
	(sizeof(struct riscv64_ucontext) == 960) ? 1 : -1];
typedef char riscv64_rt_sigframe_size_must_be_1088[
	(sizeof(struct riscv64_rt_sigframe) == 1088) ? 1 : -1];

static void riscv64_frame_to_regs(struct riscv64_user_regs *regs,
	const struct riscv64_trap_frame *frame)
{
	regs->pc = frame->sepc;
	regs->ra = frame->ra;
	regs->sp = frame->sp;
	regs->gp = frame->gp;
	regs->tp = frame->tp;
	regs->t0 = frame->t0;
	regs->t1 = frame->t1;
	regs->t2 = frame->t2;
	regs->s0 = frame->s0;
	regs->s1 = frame->s1;
	regs->a0 = frame->a0;
	regs->a1 = frame->a1;
	regs->a2 = frame->a2;
	regs->a3 = frame->a3;
	regs->a4 = frame->a4;
	regs->a5 = frame->a5;
	regs->a6 = frame->a6;
	regs->a7 = frame->a7;
	regs->s2 = frame->s2;
	regs->s3 = frame->s3;
	regs->s4 = frame->s4;
	regs->s5 = frame->s5;
	regs->s6 = frame->s6;
	regs->s7 = frame->s7;
	regs->s8 = frame->s8;
	regs->s9 = frame->s9;
	regs->s10 = frame->s10;
	regs->s11 = frame->s11;
	regs->t3 = frame->t3;
	regs->t4 = frame->t4;
	regs->t5 = frame->t5;
	regs->t6 = frame->t6;
}

static void riscv64_regs_to_frame(struct riscv64_trap_frame *frame,
	const struct riscv64_user_regs *regs)
{
	frame->ra = regs->ra;
	frame->sp = regs->sp;
	frame->gp = regs->gp;
	frame->tp = regs->tp;
	frame->t0 = regs->t0;
	frame->t1 = regs->t1;
	frame->t2 = regs->t2;
	frame->s0 = regs->s0;
	frame->s1 = regs->s1;
	frame->a0 = regs->a0;
	frame->a1 = regs->a1;
	frame->a2 = regs->a2;
	frame->a3 = regs->a3;
	frame->a4 = regs->a4;
	frame->a5 = regs->a5;
	frame->a6 = regs->a6;
	frame->a7 = regs->a7;
	frame->s2 = regs->s2;
	frame->s3 = regs->s3;
	frame->s4 = regs->s4;
	frame->s5 = regs->s5;
	frame->s6 = regs->s6;
	frame->s7 = regs->s7;
	frame->s8 = regs->s8;
	frame->s9 = regs->s9;
	frame->s10 = regs->s10;
	frame->s11 = regs->s11;
	frame->t3 = regs->t3;
	frame->t4 = regs->t4;
	frame->t5 = regs->t5;
	frame->t6 = regs->t6;
	frame->sepc = regs->pc;
	frame->stval = 0;
}

void riscv64_signal_frame_build(struct riscv64_rt_sigframe *signal_frame,
	const struct riscv64_trap_frame *frame, unsigned int signum,
	unsigned int oldmask)
{
	int *siginfo;

	memset_b(signal_frame, 0, sizeof(*signal_frame));
	siginfo = (int *)signal_frame->siginfo;
	siginfo[0] = signum;
	signal_frame->context.sigmask = oldmask;
	riscv64_frame_to_regs(&signal_frame->context.mcontext.regs, frame);
}

int riscv64_signal_frame_restore(struct riscv64_trap_frame *frame,
	const struct riscv64_rt_sigframe *signal_frame)
{
	unsigned long sstatus;

	sstatus = frame->sstatus;
	riscv64_regs_to_frame(frame, &signal_frame->context.mcontext.regs);
	frame->sstatus = (sstatus & ~(RV_SSTATUS_SIE | RV_SSTATUS_SPP |
		RV_SSTATUS_SPIE)) | RV_SSTATUS_SPIE;
	return 0;
}

static int riscv64_signal_stack_map(unsigned long start, unsigned long end)
{
	struct vma *stack;
	unsigned long address, page;

	if(!start || start >= end || end > RISCV64_USER_STACK_TOP) {
		return -EFAULT;
	}
	stack = find_vma_region(end - 1);
	if(!stack || stack->s_type != P_STACK ||
		!(stack->prot & PROT_WRITE)) {
		return -EFAULT;
	}
	start &= PAGE_MASK;
	if(start < stack->start) {
		if(stack != current->vma_table && start < stack->prev->end) {
			return -EFAULT;
		}
		stack->start = start;
	}
	for(address = start; address < end; address += PAGE_SIZE) {
		if(get_mapped_addr(current, address) & PAGE_PRESENT) {
			continue;
		}
		page = map_page(current, address, 0, stack->prot);
		if(!page) {
			return -ENOMEM;
		}
		memset_b((void *)page, 0, PAGE_SIZE);
		current->usage.ru_minflt++;
	}
	return 0;
}

int riscv64_signal_deliver(struct riscv64_trap_frame *frame,
	unsigned int signum, unsigned int oldmask)
{
	struct riscv64_rt_sigframe *signal_frame;
	struct vma *handler;
	unsigned long address;

	if(frame->sp < sizeof(*signal_frame)) {
		return -EFAULT;
	}
	address = (frame->sp - sizeof(*signal_frame)) &
		RISCV64_ALIGN16_MASK;
	if(riscv64_signal_stack_map(address, frame->sp)) {
		return -EFAULT;
	}
	handler = find_vma_region((__addr_t)
		current->sigaction[signum - 1].sa_handler);
	if(!handler || !(handler->prot & PROT_EXEC)) {
		return -EFAULT;
	}
	signal_frame = (struct riscv64_rt_sigframe *)address;
	riscv64_signal_frame_build(signal_frame, frame, signum, oldmask);
	frame->ra = RISCV64_SIGNAL_TRAMPOLINE;
	frame->sp = address;
	frame->a0 = signum;
	frame->a1 = address;
	frame->a2 = address + 128;
	frame->sepc = (__addr_t)current->sigaction[signum - 1].sa_handler;
	return 0;
}

int riscv64_signal_return(struct riscv64_trap_frame *frame)
{
	struct riscv64_rt_sigframe *signal_frame;
	struct riscv64_user_regs regs;
	struct vma *pc;
	unsigned long mask;
	int error;

	if((frame->sp & 15) || frame->sp >= RISCV64_USER_STACK_TOP ||
		frame->sp > RISCV64_USER_STACK_TOP - sizeof(*signal_frame)) {
		return -EFAULT;
	}
	signal_frame = (struct riscv64_rt_sigframe *)frame->sp;
	if((error = check_user_area(VERIFY_READ, signal_frame,
		sizeof(*signal_frame)))) {
		return error;
	}
	regs = signal_frame->context.mcontext.regs;
	pc = find_vma_region(regs.pc);
	if(!regs.pc || regs.pc >= RISCV64_USER_STACK_TOP ||
		!pc || !(pc->prot & PROT_EXEC) || !regs.sp ||
		regs.sp > RISCV64_USER_STACK_TOP) {
		return -EFAULT;
	}
	mask = signal_frame->context.sigmask;
	riscv64_signal_frame_restore(frame, signal_frame);
	current->sigblocked = (__sigset_t)mask & SIG_BLOCKABLE;
	current->sigexecuting = 0;
	return 0;
}

int riscv64_signal_map(void)
{
	unsigned int *code;
	unsigned long page;
	signed long result;

	result = do_mmap(NULL, RISCV64_SIGNAL_TRAMPOLINE, PAGE_SIZE,
		PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_FIXED, 0, P_TEXT,
		0, NULL);
	if(result < 0) {
		return result;
	}
	page = map_page(current, RISCV64_SIGNAL_TRAMPOLINE, 0,
		PROT_READ | PROT_EXEC);
	if(!page) {
		return -ENOMEM;
	}
	memset_b((void *)page, 0, PAGE_SIZE);
	code = (unsigned int *)page;
	code[0] = 0x08b00893U;	/* addi a7, zero, 139 */
	code[1] = 0x00000073U;	/* ecall */
	code[2] = 0x0000006fU;	/* jal zero, 0 */
	riscv64_fence_i();
	return 0;
}

int riscv64_rt_sigaction(unsigned long signum, const void *new_pointer,
	void *old_pointer, unsigned long sigset_size)
{
	const struct riscv64_kernel_sigaction *new_action;
	struct riscv64_kernel_sigaction *old_action;
	struct sigaction action, old;
	int error;

	if(sigset_size != RISCV64_RT_SIGSET_SIZE) {
		return -EINVAL;
	}
	if(signum >= NSIG) {
		return -EINVAL;
	}
	new_action = (const struct riscv64_kernel_sigaction *)new_pointer;
	old_action = (struct riscv64_kernel_sigaction *)old_pointer;
	if(new_action) {
		if((error = check_user_area(VERIFY_READ, new_action,
			sizeof(*new_action)))) {
			return error;
		}
		action.sa_handler = (__sighandler_t)new_action->handler;
		action.sa_flags = (int)new_action->flags;
		action.sa_mask = (__sigset_t)new_action->mask;
		action.sa_restorer = NULL;
	}
	if(old_action && (error = check_user_area(VERIFY_WRITE, old_action,
		sizeof(*old_action)))) {
		return error;
	}
	error = do_sigaction((__sigset_t)signum,
		new_action ? &action : NULL, old_action ? &old : NULL);
	if(error || !old_action) {
		return error;
	}
	old_action->handler = (unsigned long)old.sa_handler;
	old_action->flags = (unsigned long)(unsigned int)old.sa_flags;
	old_action->mask = old.sa_mask;
	return 0;
}

int riscv64_rt_sigprocmask(unsigned long how, const void *set_pointer,
	void *old_pointer, unsigned long sigset_size)
{
	const unsigned long *set;
	unsigned long *oldset;
	unsigned long value;
	int error;

	if(sigset_size != RISCV64_RT_SIGSET_SIZE) {
		return -EINVAL;
	}
	set = (const unsigned long *)set_pointer;
	oldset = (unsigned long *)old_pointer;
	if(oldset) {
		if((error = check_user_area(VERIFY_WRITE, oldset, sizeof(*oldset)))) {
			return error;
		}
		*oldset = current->sigblocked;
	}
	if(!set) {
		return 0;
	}
	if((error = check_user_area(VERIFY_READ, set, sizeof(*set)))) {
		return error;
	}
	value = *set & SIG_BLOCKABLE;
	switch(how) {
		case SIG_BLOCK:
			current->sigblocked |= (__sigset_t)value;
			break;
		case SIG_UNBLOCK:
			current->sigblocked &= ~(__sigset_t)value;
			break;
		case SIG_SETMASK:
			current->sigblocked = (__sigset_t)value;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}
