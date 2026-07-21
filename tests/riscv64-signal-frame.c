#include <fiwix/errno.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/process.h>
#include <fiwix/riscv64_signal.h>
#include <fiwix/string.h>

static struct proc process;
struct proc *current = &process;
static struct vma stack_vma;
static struct vma text_vma;
static unsigned char user_stack[4096] __attribute__((aligned(4096)));
static unsigned char trampoline_page[4096] __attribute__((aligned(4096)));
static int mmap_calls;
static int fence_calls;

static void handler_marker(int signum)
{
	(void)signum;
}

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
	if(owner != current) {
		return 0;
	}
	if(address == RISCV64_SIGNAL_TRAMPOLINE) {
		return (__addr_t)trampoline_page;
	}
	return 0;
}

signed long do_mmap(struct inode *inode, __addr_t start, __size_t length,
	unsigned int prot, unsigned int flags, unsigned int offset, char type,
	char mode, void *object)
{
	(void)inode;
	(void)offset;
	(void)mode;
	(void)object;
	if(start != RISCV64_SIGNAL_TRAMPOLINE || length != PAGE_SIZE ||
		prot != (PROT_READ | PROT_EXEC) ||
		flags != (MAP_PRIVATE | MAP_FIXED) || type != P_TEXT) {
		return -EINVAL;
	}
	mmap_calls++;
	return (signed long)start;
}

int check_user_area(int type, const void *address, unsigned int size)
{
	(void)type;
	(void)address;
	(void)size;
	return 0;
}

void riscv64_fence_i(void)
{
	fence_calls++;
}

void memset_b(void *destination, unsigned char value, unsigned int count)
{
	unsigned char *byte;

	byte = (unsigned char *)destination;
	while(count--) {
		*byte++ = value;
	}
}

int main(void)
{
	struct riscv64_rt_sigframe signal_frame;
	struct riscv64_trap_frame source, restored;
	struct riscv64_kernel_sigaction action, old_action;
	struct riscv64_rt_sigframe *user_frame;
	unsigned long signal_set, old_set;
	unsigned long *source_word;
	unsigned long *user_word;
	unsigned long original_sp;
	unsigned int n;

	if(sizeof(struct riscv64_kernel_sigaction) != 24 ||
		sizeof(struct riscv64_user_regs) != 256 ||
		sizeof(struct riscv64_sigcontext) != 784 ||
		sizeof(struct riscv64_ucontext) != 960 ||
		sizeof(struct riscv64_rt_sigframe) != 1088 ||
		__builtin_offsetof(struct riscv64_ucontext, sigmask) != 40 ||
		__builtin_offsetof(struct riscv64_ucontext, mcontext) != 176 ||
		__builtin_offsetof(struct riscv64_rt_sigframe, context) != 128) {
		return 1;
	}

	memset_b(&source, 0, sizeof(source));
	source_word = (unsigned long *)&source;
	for(n = 0; n < 31; n++) {
		source_word[n] = 0x1000 + n;
	}
	source.sepc = 0x2000;
	source.sstatus = 0x40020;
	source.stval = 0x3000;
	riscv64_signal_frame_build(&signal_frame, &source, 11, 0x1234);
	if(((int *)signal_frame.siginfo)[0] != 11 ||
		signal_frame.context.sigmask != 0x1234 ||
		signal_frame.context.mcontext.regs.pc != source.sepc) {
		return 2;
	}
	user_word = (unsigned long *)&signal_frame.context.mcontext.regs;
	for(n = 1; n < 32; n++) {
		if(user_word[n] != source_word[n - 1]) {
			return 3;
		}
	}

	memset_b(&restored, 0, sizeof(restored));
	restored.sstatus = 0x40000 | 0x100 | 0x2;
	if(riscv64_signal_frame_restore(&restored, &signal_frame)) {
		return 4;
	}
	if(restored.sepc != source.sepc || restored.ra != source.ra ||
		restored.sp != source.sp || restored.a0 != source.a0 ||
		restored.t6 != source.t6 || restored.stval ||
		(restored.sstatus & (0x100 | 0x2)) ||
		!(restored.sstatus & 0x20)) {
		return 5;
	}

	memset_b(&process, 0, sizeof(process));
	memset_b(&action, 0, sizeof(action));
	action.handler = (unsigned long)handler_marker;
	action.flags = SA_SIGINFO | SA_RESTART;
	action.mask = 1U << (SIGUSR2 - 1);
	if(riscv64_rt_sigaction(SIGUSR1, &action, &old_action, 4) != -EINVAL ||
		riscv64_rt_sigaction(SIGKILL, &action, &old_action,
		RISCV64_RT_SIGSET_SIZE) != -EINVAL ||
		riscv64_rt_sigaction(SIGUSR1, &action, &old_action,
		RISCV64_RT_SIGSET_SIZE) || old_action.handler ||
		current->sigaction[SIGUSR1 - 1].sa_handler != handler_marker ||
		current->sigaction[SIGUSR1 - 1].sa_flags !=
			(SA_SIGINFO | SA_RESTART) ||
		current->sigaction[SIGUSR1 - 1].sa_mask != action.mask) {
		return 6;
	}

	current->sigblocked = 0;
	signal_set = ~0UL;
	old_set = ~0UL;
	if(riscv64_rt_sigprocmask(SIG_SETMASK, &signal_set, &old_set,
		RISCV64_RT_SIGSET_SIZE) || old_set ||
		(current->sigblocked & (1U << (SIGKILL - 1))) ||
		(current->sigblocked & (1U << (SIGSTOP - 1))) ||
		!(current->sigblocked & (1U << (SIGUSR1 - 1)))) {
		return 7;
	}

	memset_b(&stack_vma, 0, sizeof(stack_vma));
	stack_vma.start = (__addr_t)user_stack;
	stack_vma.end = stack_vma.start + sizeof(user_stack);
	stack_vma.prot = PROT_READ | PROT_WRITE;
	stack_vma.s_type = P_STACK;
	stack_vma.prev = &stack_vma;
	memset_b(&text_vma, 0, sizeof(text_vma));
	text_vma.start = (unsigned long)handler_marker & PAGE_MASK;
	text_vma.end = text_vma.start + PAGE_SIZE;
	text_vma.prot = PROT_READ | PROT_EXEC;
	memset_b(&source, 0, sizeof(source));
	original_sp = stack_vma.end & ~15UL;
	source.sp = original_sp;
	source.sepc = (unsigned long)handler_marker;
	source.ra = 0x1234;
	source.a0 = 0x5678;
	source.sstatus = 0x40020;
	current->sigaction[SIGUSR1 - 1].sa_handler = handler_marker;
	if(riscv64_signal_deliver(&source, SIGUSR1, 0x55)) {
		return 8;
	}
	user_frame = (struct riscv64_rt_sigframe *)source.sp;
	if(source.ra != RISCV64_SIGNAL_TRAMPOLINE ||
		source.sepc != (unsigned long)handler_marker ||
		source.a0 != SIGUSR1 || source.a1 != source.sp ||
		source.a2 != source.sp + 128 ||
		user_frame->context.sigmask != 0x55 ||
		user_frame->context.mcontext.regs.sp != original_sp ||
		user_frame->context.mcontext.regs.a0 != 0x5678) {
		return 9;
	}
	current->sigblocked = ~0U;
	if(riscv64_signal_return(&source) || source.sp != original_sp ||
		source.a0 != 0x5678 || source.ra != 0x1234 ||
		current->sigblocked != (0x55 & SIG_BLOCKABLE) ||
		current->sigexecuting) {
		return 10;
	}

	source.sp = (__addr_t)user_frame;
	user_frame->context.mcontext.regs.pc = 0xdead;
	if(riscv64_signal_return(&source) != -EFAULT) {
		return 11;
	}

	memset_b(trampoline_page, 0xa5, sizeof(trampoline_page));
	mmap_calls = fence_calls = 0;
	if(riscv64_signal_map() || mmap_calls != 1 || fence_calls != 1 ||
		((unsigned int *)trampoline_page)[0] != 0x08b00893U ||
		((unsigned int *)trampoline_page)[1] != 0x00000073U ||
		((unsigned int *)trampoline_page)[2] != 0x0000006fU ||
		trampoline_page[PAGE_SIZE - 1]) {
		return 12;
	}

	return 0;
}
