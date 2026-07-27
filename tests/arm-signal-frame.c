/*
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_signal.h>
#include <fiwix/errno.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/process.h>
#include <fiwix/signal.h>
#include <fiwix/string.h>

#define HOST_STACK		0x30000000U
#define HOST_TRAMPOLINE		0x31000000U
#define HOST_HANDLER		0x00101000U
#define HOST_PROT_READ		0x1
#define HOST_PROT_WRITE		0x2
#define HOST_MAP_PRIVATE	0x2
#define HOST_MAP_ANONYMOUS	0x20
#define HOST_MAP_FIXED_NOREPLACE 0x100000

extern void *mmap(void *, unsigned long, int, int, int, long);
extern int munmap(void *, unsigned long);

static struct proc process;
static struct vma stack_vma;
static struct vma text_vma;
static int mmap_calls;

struct proc *current = &process;

struct vma *find_vma_region(__addr_t address)
{
	if(address >= stack_vma.start && address < stack_vma.end) {
		return &stack_vma;
	}
	if(address >= text_vma.start && address < text_vma.end) {
		return &text_vma;
	}
	return 0;
}

__addr_t get_mapped_addr(struct proc *owner, __addr_t address)
{
	if(owner == current && address >= stack_vma.start &&
		address < stack_vma.end) {
		return address | PAGE_PRESENT;
	}
	return 0;
}

__addr_t map_page(struct proc *owner, __addr_t address, __addr_t physical,
	unsigned int prot)
{
	(void)physical;
	(void)prot;
	if(owner != current || address != ARM_SIGNAL_TRAMPOLINE) {
		return 0;
	}
	return HOST_TRAMPOLINE;
}

signed long do_mmap(struct inode *inode, __addr_t start, __size_t length,
	unsigned int prot, unsigned int flags, unsigned int offset, char type,
	char mode, void *object)
{
	(void)inode;
	(void)offset;
	(void)mode;
	(void)object;
	if(start != ARM_SIGNAL_TRAMPOLINE || length != PAGE_SIZE ||
		prot != (PROT_READ | PROT_EXEC) ||
		flags != (MAP_PRIVATE | MAP_FIXED) || type != P_TEXT) {
		return -EINVAL;
	}
	mmap_calls++;
	return start;
}

int check_user_area(int type, const void *address, unsigned int size)
{
	(void)type;
	(void)address;
	(void)size;
	return 0;
}

void memset_b(void *destination, unsigned char value, unsigned int count)
{
	unsigned char *byte;

	byte = (unsigned char *)destination;
	while(count--) {
		*byte++ = value;
	}
}

static int map_host_pages(void)
{
	if(mmap((void *)(unsigned long)HOST_STACK, PAGE_SIZE,
			HOST_PROT_READ | HOST_PROT_WRITE,
			HOST_MAP_PRIVATE | HOST_MAP_ANONYMOUS |
				HOST_MAP_FIXED_NOREPLACE, -1, 0) !=
		(void *)(unsigned long)HOST_STACK) {
		return -1;
	}
	if(mmap((void *)(unsigned long)HOST_TRAMPOLINE, PAGE_SIZE,
			HOST_PROT_READ | HOST_PROT_WRITE,
			HOST_MAP_PRIVATE | HOST_MAP_ANONYMOUS |
				HOST_MAP_FIXED_NOREPLACE, -1, 0) !=
		(void *)(unsigned long)HOST_TRAMPOLINE) {
		munmap((void *)(unsigned long)HOST_STACK, PAGE_SIZE);
		return -1;
	}
	return 0;
}

int main(void)
{
	struct arm_rt_sigframe signal_frame;
	struct arm_rt_sigframe *user_frame;
	struct arm_trap_frame source;
	struct arm_trap_frame restored;
	struct arm_kernel_sigaction action;
	struct arm_kernel_sigaction old_action;
	unsigned int signal_set[2];
	unsigned int old_set[2];
	unsigned int original_sp;
	unsigned int n;

	if(sizeof(struct arm_kernel_sigaction) != 20 ||
		sizeof(struct arm_sigcontext) != 84 ||
		sizeof(struct arm_ucontext) != 744 ||
		sizeof(struct arm_sigframe) != 760 ||
		sizeof(struct arm_rt_sigframe) != 888 ||
		__builtin_offsetof(struct arm_ucontext, mcontext) != 20 ||
		__builtin_offsetof(struct arm_ucontext, sigmask) != 104 ||
		__builtin_offsetof(struct arm_ucontext, regspace) != 232 ||
		__builtin_offsetof(struct arm_rt_sigframe, signal) != 128) {
		return 1;
	}
	memset_b(&source, 0, sizeof(source));
	for(n = 0; n < 13; n++) {
		source.r[n] = 0x1000U + n;
	}
	source.pc = HOST_HANDLER;
	source.cpsr = 0xA80F0050U;
	source.user_sp = HOST_STACK + PAGE_SIZE;
	source.user_lr = 0x12345678U;
	source.vector = 4;
	arm_signal_frame_build(&signal_frame, &source, SIGSEGV, 0x1234);
	if(((int *)signal_frame.siginfo)[0] != SIGSEGV ||
		signal_frame.signal.context.sigmask[0] != 0x1234 ||
		signal_frame.signal.context.mcontext.r[0] != source.r[0] ||
		signal_frame.signal.context.mcontext.fp != source.r[11] ||
		signal_frame.signal.context.mcontext.ip != source.r[12] ||
		signal_frame.signal.context.mcontext.pc != source.pc ||
		signal_frame.signal.context.mcontext.trap_no != source.vector ||
		signal_frame.signal.retcode[0] != 0xE3A070ADU ||
		signal_frame.signal.retcode[1] != 0xEF000000U) {
		return 2;
	}
	memset_b(&restored, 0, sizeof(restored));
	if(arm_signal_frame_restore(&restored, &signal_frame) ||
		restored.r[0] != source.r[0] ||
		restored.r[11] != source.r[11] ||
		restored.r[12] != source.r[12] ||
		restored.pc != source.pc ||
		restored.user_sp != source.user_sp ||
		restored.user_lr != source.user_lr ||
		restored.cpsr != source.cpsr || restored.vector) {
		return 3;
	}

	memset_b(&process, 0, sizeof(process));
	memset_b(&action, 0, sizeof(action));
	action.handler = HOST_HANDLER;
	action.flags = SA_SIGINFO | SA_RESTART | ARM_SA_RESTORER;
	action.restorer = 0x2000;
	action.mask[0] = 1U << (SIGUSR2 - 1);
	if(arm_rt_sigaction(SIGUSR1, &action, &old_action, 4) != -EINVAL ||
		arm_rt_sigaction(SIGKILL, &action, &old_action,
			ARM_RT_SIGSET_SIZE) != -EINVAL ||
		arm_rt_sigaction(SIGUSR1, &action, &old_action,
			ARM_RT_SIGSET_SIZE) || old_action.handler ||
		(unsigned int)(unsigned long)
			current->sigaction[SIGUSR1 - 1].sa_handler !=
			HOST_HANDLER ||
		current->sigaction[SIGUSR1 - 1].sa_flags !=
			(int)action.flags ||
		current->sigaction[SIGUSR1 - 1].sa_mask != action.mask[0] ||
		(unsigned int)(unsigned long)
			current->sigaction[SIGUSR1 - 1].sa_restorer !=
			action.restorer) {
		return 4;
	}

	signal_set[0] = signal_set[1] = ~0U;
	old_set[0] = old_set[1] = ~0U;
	if(arm_rt_sigprocmask(SIG_SETMASK, signal_set, old_set,
			ARM_RT_SIGSET_SIZE) || old_set[0] || old_set[1] ||
		(current->sigblocked & (1U << (SIGKILL - 1))) ||
		(current->sigblocked & (1U << (SIGSTOP - 1))) ||
		!(current->sigblocked & (1U << (SIGUSR1 - 1)))) {
		return 5;
	}
	if(map_host_pages()) {
		return 6;
	}
	memset_b(&stack_vma, 0, sizeof(stack_vma));
	stack_vma.start = HOST_STACK;
	stack_vma.end = HOST_STACK + PAGE_SIZE;
	stack_vma.prot = PROT_READ | PROT_WRITE;
	stack_vma.s_type = P_STACK;
	stack_vma.prev = &stack_vma;
	memset_b(&text_vma, 0, sizeof(text_vma));
	text_vma.start = HOST_HANDLER & PAGE_MASK;
	text_vma.end = text_vma.start + PAGE_SIZE;
	text_vma.prot = PROT_READ | PROT_EXEC;
	original_sp = stack_vma.end;
	source.user_sp = original_sp;
	source.pc = HOST_HANDLER;
	source.r[0] = 0x55667788U;
	source.cpsr = 0xA80F0050U;
	current->sigaction[SIGUSR1 - 1].sa_handler =
		(__sighandler_t)(unsigned long)HOST_HANDLER;
	if(arm_signal_deliver(&source, SIGUSR1, 0x55)) {
		return 7;
	}
	user_frame = (struct arm_rt_sigframe *)(unsigned long)source.user_sp;
	if(source.user_lr != ARM_SIGNAL_TRAMPOLINE ||
		source.pc != HOST_HANDLER || source.r[0] != SIGUSR1 ||
		source.r[1] != source.user_sp ||
		source.r[2] != source.user_sp + 128 ||
		user_frame->signal.context.sigmask[0] != 0x55 ||
		user_frame->signal.context.mcontext.sp != original_sp ||
		user_frame->signal.context.mcontext.r[0] != 0x55667788U) {
		return 8;
	}
	current->sigblocked = ~0U;
	if(arm_signal_return(&source) || source.user_sp != original_sp ||
		source.r[0] != 0x55667788U ||
		current->sigblocked != (0x55 & SIG_BLOCKABLE) ||
		current->sigexecuting) {
		return 9;
	}
	source.user_sp = (unsigned int)(unsigned long)user_frame;
	user_frame->signal.context.mcontext.pc = 0xDEACU;
	if(arm_signal_return(&source) != -EFAULT) {
		return 10;
	}
	memset_b((void *)(unsigned long)HOST_TRAMPOLINE, 0xA5, PAGE_SIZE);
	mmap_calls = 0;
	if(arm_signal_map() || mmap_calls != 1 ||
		((unsigned int *)(unsigned long)HOST_TRAMPOLINE)[0] !=
			0xE3A070ADU ||
		((unsigned int *)(unsigned long)HOST_TRAMPOLINE)[1] !=
			0xEF000000U ||
		((unsigned int *)(unsigned long)HOST_TRAMPOLINE)[2] !=
			0xEAFFFFFEU ||
		*((unsigned char *)(unsigned long)
			(HOST_TRAMPOLINE + PAGE_SIZE - 1))) {
		return 11;
	}
	if(munmap((void *)(unsigned long)HOST_STACK, PAGE_SIZE) ||
		munmap((void *)(unsigned long)HOST_TRAMPOLINE, PAGE_SIZE)) {
		return 12;
	}
	return 0;
}
