/* Generic Fiwix process hooks for riscv64 kernel tasks. */

#include <fiwix/mm.h>
#include <fiwix/process.h>

int riscv64_process_setup(struct proc *p, int (*fn)(void))
{
	__addr_t stack;

	stack = kmalloc(PAGE_SIZE);
	if(!stack) {
		return -1;
	}
	p->arch.kernel_sp = stack;
	p->arch.sp = (stack + PAGE_SIZE) & ~15UL;
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
	p->arch.sp = (stack + PAGE_SIZE) & ~15UL;
	p->arch.ra = (unsigned long)riscv64_user_process_entry;
	p->arch.s0 = entry;
	p->arch.s1 = user_sp;
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
