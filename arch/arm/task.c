/*
 * fiwix/arch/arm/task.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_trap.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/string.h>

#define ARM_STACK_ALIGN_MASK	0xFFFFFFF8U

extern void arm_kernel_process_entry(void);
extern void arm_user_process_entry(void);
extern void arm_return_to_user(void);

int arm_process_setup(struct proc *p, int (*fn)(void))
{
	__addr_t stack;

	if(arm_process_address_space_create(p, 0) < 0) {
		return -1;
	}
	stack = kmalloc(PAGE_SIZE);
	if(!stack) {
		arm_process_address_space_release(p);
		return -1;
	}
	p->arch.kernel_sp = stack;
	p->arch.sp = (stack + PAGE_SIZE) & ARM_STACK_ALIGN_MASK;
	p->arch.lr = (unsigned int)arm_kernel_process_entry;
	p->arch.r4 = (unsigned int)fn;
	p->rss++;
	return 0;
}

int arm_user_process_setup(struct proc *p, unsigned int entry,
	unsigned int user_sp)
{
	__addr_t stack;

	if(!arm_process_root(p)) {
		return -1;
	}
	stack = kmalloc(PAGE_SIZE);
	if(!stack) {
		return -1;
	}
	p->arch.kernel_sp = stack;
	p->arch.sp = (stack + PAGE_SIZE) & ARM_STACK_ALIGN_MASK;
	p->arch.lr = (unsigned int)arm_user_process_entry;
	p->arch.r4 = entry;
	p->arch.r5 = user_sp;
	p->rss++;
	return 0;
}

int arm_fork_process_setup(struct proc *p,
	struct arm_trap_frame *parent_frame)
{
	struct arm_trap_frame *child_frame;
	__addr_t stack;

	if(arm_process_address_space_create(p, current) < 0) {
		return -1;
	}
	stack = kmalloc(PAGE_SIZE);
	if(!stack) {
		arm_process_address_space_release(p);
		return -1;
	}
	child_frame = (struct arm_trap_frame *)(stack + PAGE_SIZE -
		sizeof(struct arm_trap_frame));
	memcpy_b(child_frame, parent_frame, sizeof(struct arm_trap_frame));
	child_frame->r[0] = 0;

	p->arch.kernel_sp = stack;
	p->arch.sp = (unsigned int)child_frame;
	p->arch.lr = (unsigned int)arm_return_to_user;
	p->rss++;
	return 0;
}

void arm_process_release(struct proc *p)
{
	if(p->arch.kernel_sp) {
		kfree(p->arch.kernel_sp);
		p->arch.kernel_sp = 0;
		p->rss--;
	}
	arm_process_address_space_release(p);
}
