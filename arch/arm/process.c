/* Generic Fiwix process address-space hooks for ARMv7. */

#include <fiwix/arm_vm.h>
#include <fiwix/config.h>
#include <fiwix/process.h>

static unsigned int arm_process_roots[NR_PROCS][ARM_VM_ROOT_ENTRIES]
	__attribute__((aligned(ARM_VM_ROOT_ALIGNMENT)));
static struct proc *arm_process_root_owners[NR_PROCS];

void arm_process_roots_init(void)
{
	unsigned int n;

	for(n = 0; n < NR_PROCS; n++) {
		arm_process_root_owners[n] = 0;
	}
}

unsigned int *arm_process_root(const struct proc *p)
{
	unsigned int n;

	if(!p || !p->arch.ttbr0) {
		return 0;
	}
	for(n = 0; n < NR_PROCS; n++) {
		if(arm_process_root_owners[n] == p &&
			arm_vm_ttbr0(arm_process_roots[n]) ==
				p->arch.ttbr0) {
			return arm_process_roots[n];
		}
	}
	return 0;
}

int arm_process_address_space_create(struct proc *p,
	const struct proc *parent)
{
	unsigned int *parent_root;
	unsigned int n;

	if(!p || p->arch.ttbr0) {
		return -1;
	}
	parent_root = 0;
	if(parent) {
		parent_root = arm_process_root(parent);
		if(!parent_root) {
			return -1;
		}
	}
	for(n = 0; n < NR_PROCS; n++) {
		if(!arm_process_root_owners[n]) {
			break;
		}
	}
	if(n == NR_PROCS) {
		return -1;
	}
	if(parent_root) {
		if(arm_vm_root_clone(arm_process_roots[n], parent_root)) {
			return -1;
		}
	} else if(arm_vm_root_init(arm_process_roots[n])) {
		return -1;
	}
	p->arch.ttbr0 = arm_vm_ttbr0(arm_process_roots[n]);
	if(!p->arch.ttbr0) {
		return -1;
	}
	arm_process_root_owners[n] = p;
	return 0;
}

int arm_process_address_space_release(struct proc *p)
{
	unsigned int *root;
	unsigned int n;
	unsigned int word;

	root = arm_process_root(p);
	if(!root) {
		return -1;
	}
	for(n = 0; n < NR_PROCS; n++) {
		if(arm_process_root_owners[n] == p &&
			arm_process_roots[n] == root) {
			break;
		}
	}
	if(n == NR_PROCS) {
		return -1;
	}
	for(word = 0; word < ARM_VM_ROOT_ENTRIES; word++) {
		root[word] = 0;
	}
	arm_process_root_owners[n] = 0;
	p->arch.ttbr0 = 0;
	return 0;
}

int arm_process_context_activate(const struct proc *p)
{
	unsigned int *root;

	root = arm_process_root(p);
	return root ? arm_vm_context_activate(root) : -1;
}
