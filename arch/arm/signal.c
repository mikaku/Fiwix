/*
 * fiwix/arch/arm/signal.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_signal.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/process.h>
#include <fiwix/signal.h>
#include <fiwix/string.h>
#include <fiwix/syscalls.h>

#define ARM_ALIGN8_MASK		0xFFFFFFF8U
#define ARM_USER_CPSR		0x00000050U
#define ARM_USER_STATUS_MASK	0xF80F0000U
#define ARM_RT_SIGRETURN_MOV	0xE3A070ADU
#define ARM_SVC_ZERO		0xEF000000U
#define ARM_BRANCH_SELF		0xEAFFFFFEU

typedef char arm_kernel_sigaction_size_must_be_20[
	(sizeof(struct arm_kernel_sigaction) == 20) ? 1 : -1];
typedef char arm_sigcontext_size_must_be_84[
	(sizeof(struct arm_sigcontext) == 84) ? 1 : -1];
typedef char arm_ucontext_size_must_be_744[
	(sizeof(struct arm_ucontext) == 744) ? 1 : -1];
typedef char arm_rt_sigframe_size_must_be_888[
	(sizeof(struct arm_rt_sigframe) == 888) ? 1 : -1];

static void arm_frame_to_context(struct arm_sigcontext *context,
	const struct arm_trap_frame *frame)
{
	unsigned int n;

	context->trap_no = frame->vector;
	for(n = 0; n < 11; n++) {
		context->r[n] = frame->r[n];
	}
	context->fp = frame->r[11];
	context->ip = frame->r[12];
	context->sp = frame->user_sp;
	context->lr = frame->user_lr;
	context->pc = frame->pc;
	context->cpsr = frame->cpsr;
}

static void arm_context_to_frame(struct arm_trap_frame *frame,
	const struct arm_sigcontext *context)
{
	unsigned int n;

	for(n = 0; n < 11; n++) {
		frame->r[n] = context->r[n];
	}
	frame->r[11] = context->fp;
	frame->r[12] = context->ip;
	frame->user_sp = context->sp;
	frame->user_lr = context->lr;
	frame->pc = context->pc;
	frame->cpsr = (context->cpsr & ARM_USER_STATUS_MASK) |
		ARM_USER_CPSR;
	frame->vector = 0;
}

void arm_signal_frame_build(struct arm_rt_sigframe *signal_frame,
	const struct arm_trap_frame *frame, unsigned int signum,
	unsigned int oldmask)
{
	int *siginfo;

	memset_b(signal_frame, 0, sizeof(*signal_frame));
	siginfo = (int *)signal_frame->siginfo;
	siginfo[0] = signum;
	signal_frame->signal.context.sigmask[0] = oldmask;
	arm_frame_to_context(&signal_frame->signal.context.mcontext, frame);
	signal_frame->signal.retcode[0] = ARM_RT_SIGRETURN_MOV;
	signal_frame->signal.retcode[1] = ARM_SVC_ZERO;
	signal_frame->signal.retcode[2] = ARM_BRANCH_SELF;
}

int arm_signal_frame_restore(struct arm_trap_frame *frame,
	const struct arm_rt_sigframe *signal_frame)
{
	arm_context_to_frame(frame, &signal_frame->signal.context.mcontext);
	return 0;
}

static int arm_signal_stack_map(unsigned int start, unsigned int end)
{
	struct vma *stack;
	unsigned int address;
	__addr_t page;

	if(!start || start >= end || end > ARM_USER_STACK_TOP) {
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
		memset_b((void *)(unsigned long)page, 0, PAGE_SIZE);
		current->usage.ru_minflt++;
	}
	return 0;
}

int arm_signal_deliver(struct arm_trap_frame *frame, unsigned int signum,
	unsigned int oldmask)
{
	struct arm_rt_sigframe *signal_frame;
	struct vma *handler;
	unsigned int address;
	unsigned int handler_address;

	if(frame->user_sp < sizeof(*signal_frame)) {
		return -EFAULT;
	}
	address = (frame->user_sp - sizeof(*signal_frame)) & ARM_ALIGN8_MASK;
	if(arm_signal_stack_map(address, frame->user_sp)) {
		return -EFAULT;
	}
	handler_address = (unsigned int)(unsigned long)
		current->sigaction[signum - 1].sa_handler;
	handler = find_vma_region(handler_address);
	if(!handler_address || (handler_address & 3U) ||
		!handler || !(handler->prot & PROT_EXEC)) {
		return -EFAULT;
	}
	signal_frame = (struct arm_rt_sigframe *)(unsigned long)address;
	arm_signal_frame_build(signal_frame, frame, signum, oldmask);
	frame->r[0] = signum;
	frame->r[1] = address;
	frame->r[2] = address + 128;
	frame->user_sp = address;
	frame->user_lr = ARM_SIGNAL_TRAMPOLINE;
	frame->pc = handler_address;
	frame->cpsr = (frame->cpsr & ARM_USER_STATUS_MASK) | ARM_USER_CPSR;
	return 0;
}

int arm_signal_return(struct arm_trap_frame *frame)
{
	struct arm_rt_sigframe *signal_frame;
	struct arm_sigcontext context;
	struct vma *pc;
	unsigned int address;
	unsigned int mask;
	int error;

	address = frame->user_sp;
	if((address & 7U) || address >= ARM_USER_STACK_TOP ||
		address > ARM_USER_STACK_TOP - sizeof(*signal_frame)) {
		return -EFAULT;
	}
	signal_frame = (struct arm_rt_sigframe *)(unsigned long)address;
	if((error = check_user_area(VERIFY_READ, signal_frame,
		sizeof(*signal_frame)))) {
		return error;
	}
	context = signal_frame->signal.context.mcontext;
	pc = find_vma_region(context.pc);
	if(!context.pc || (context.pc & 3U) ||
		context.pc >= ARM_USER_STACK_TOP ||
		!pc || !(pc->prot & PROT_EXEC) ||
		!context.sp || (context.sp & 7U) ||
		context.sp > ARM_USER_STACK_TOP ||
		(context.cpsr & 0x3FU) != 0x10U) {
		return -EFAULT;
	}
	mask = signal_frame->signal.context.sigmask[0];
	arm_signal_frame_restore(frame, signal_frame);
	current->sigblocked = mask & SIG_BLOCKABLE;
	current->sigexecuting = 0;
	return 0;
}

int arm_signal_map(void)
{
	unsigned int *code;
	__addr_t page;
	signed long result;

	result = do_mmap(0, ARM_SIGNAL_TRAMPOLINE, PAGE_SIZE,
		PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_FIXED, 0,
		P_TEXT, 0, 0);
	if(result < 0) {
		return result;
	}
	page = map_page(current, ARM_SIGNAL_TRAMPOLINE, 0,
		PROT_READ | PROT_EXEC);
	if(!page) {
		return -ENOMEM;
	}
	memset_b((void *)(unsigned long)page, 0, PAGE_SIZE);
	code = (unsigned int *)(unsigned long)page;
	code[0] = ARM_RT_SIGRETURN_MOV;
	code[1] = ARM_SVC_ZERO;
	code[2] = ARM_BRANCH_SELF;
	return 0;
}

int arm_rt_sigaction(unsigned int signum, const void *new_pointer,
	void *old_pointer, unsigned int sigset_size)
{
	const struct arm_kernel_sigaction *new_action;
	struct arm_kernel_sigaction *old_action;
	struct sigaction action;
	struct sigaction old;
	int error;

	if(sigset_size != ARM_RT_SIGSET_SIZE) {
		return -EINVAL;
	}
	new_action = (const struct arm_kernel_sigaction *)new_pointer;
	old_action = (struct arm_kernel_sigaction *)old_pointer;
	if(new_action) {
		if((error = check_user_area(VERIFY_READ, new_action,
			sizeof(*new_action)))) {
			return error;
		}
		action.sa_handler = (__sighandler_t)(unsigned long)
			new_action->handler;
		action.sa_flags = (int)new_action->flags;
		action.sa_mask = new_action->mask[0];
		action.sa_restorer = (void (*)(void))(unsigned long)
			new_action->restorer;
	}
	if(old_action && (error = check_user_area(VERIFY_WRITE, old_action,
		sizeof(*old_action)))) {
		return error;
	}
	error = do_sigaction(signum, new_action ? &action : 0,
		old_action ? &old : 0);
	if(error || !old_action) {
		return error;
	}
	old_action->handler = (unsigned int)(unsigned long)old.sa_handler;
	old_action->flags = (unsigned int)old.sa_flags;
	old_action->restorer = (unsigned int)(unsigned long)old.sa_restorer;
	old_action->mask[0] = old.sa_mask;
	old_action->mask[1] = 0;
	return 0;
}

int arm_rt_sigprocmask(unsigned int how, const void *set_pointer,
	void *old_pointer, unsigned int sigset_size)
{
	const unsigned int *set;
	unsigned int *oldset;
	unsigned int value;
	int error;

	if(sigset_size != ARM_RT_SIGSET_SIZE) {
		return -EINVAL;
	}
	set = (const unsigned int *)set_pointer;
	oldset = (unsigned int *)old_pointer;
	if(oldset) {
		if((error = check_user_area(VERIFY_WRITE, oldset,
			ARM_RT_SIGSET_SIZE))) {
			return error;
		}
		oldset[0] = current->sigblocked;
		oldset[1] = 0;
	}
	if(!set) {
		return 0;
	}
	if((error = check_user_area(VERIFY_READ, set,
		ARM_RT_SIGSET_SIZE))) {
		return error;
	}
	value = set[0] & SIG_BLOCKABLE;
	switch(how) {
		case SIG_BLOCK:
			current->sigblocked |= value;
			break;
		case SIG_UNBLOCK:
			current->sigblocked &= ~value;
			break;
		case SIG_SETMASK:
			current->sigblocked = value;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}
