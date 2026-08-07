/* Generic Fiwix process hooks for riscv64 kernel tasks. */

#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/riscv64_trap.h>
#include <fiwix/string.h>

#define RISCV64_ALIGN16_MASK	0xFFFFFFFFFFFFFFF0UL

int riscv64_process_setup(struct proc *p, int (*fn)(void))
{
	__addr_t stack;

	stack = kmalloc(PAGE_SIZE);
	if(!stack) {
		return -1;
	}
	p->arch.kernel_sp = stack;
	p->arch.sp = (stack + PAGE_SIZE) & RISCV64_ALIGN16_MASK;
	p->arch.ra = (unsigned long)riscv64_kernel_process_entry;
	p->arch.s0 = (unsigned long)fn;
	p->arch.satp = riscv64_read_satp();
	p->rss++;
	return 0;
}

int riscv64_user_process_setup(struct proc *p, unsigned long entry,
	unsigned long user_sp)
{
	__addr_t stack;

	stack = kmalloc(PAGE_SIZE);
	if(!stack) {
		return -1;
	}
	p->arch.kernel_sp = stack;
	p->arch.sp = (stack + PAGE_SIZE) & RISCV64_ALIGN16_MASK;
	p->arch.ra = (unsigned long)riscv64_user_process_entry;
	p->arch.s0 = entry;
	p->arch.s1 = user_sp;
	p->rss++;
	return 0;
}

int riscv64_fork_process_setup(struct proc *p,
	struct riscv64_trap_frame *parent_frame)
{
	struct riscv64_trap_frame *child_frame;
	__addr_t stack;

	if(riscv64_address_space_create(p) < 0) {
		return -1;
	}
	stack = kmalloc(PAGE_SIZE);
	if(!stack) {
		riscv64_address_space_release(p);
		return -1;
	}
	child_frame = (struct riscv64_trap_frame *)(stack + PAGE_SIZE -
		sizeof(struct riscv64_trap_frame));
	memcpy_b(child_frame, parent_frame, sizeof(struct riscv64_trap_frame));
	child_frame->a0 = 0;

	p->arch.kernel_sp = stack;
	p->arch.sp = (unsigned long)child_frame;
	p->arch.ra = (unsigned long)riscv64_return_to_user;
	p->arch.s0 = stack + PAGE_SIZE;
	p->rss++;
	return 0;
}

void riscv64_process_release(struct proc *p)
{
	if(p->arch.kernel_sp) {
		kfree(p->arch.kernel_sp);
		p->arch.kernel_sp = 0;
		p->rss--;
	}
	riscv64_address_space_release(p);
}
