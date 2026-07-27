/*
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_vm.h>
#include <fiwix/config.h>
#include <fiwix/process.h>

static struct proc processes[NR_PROCS + 1];

int main(void)
{
	struct proc forged;
	unsigned int *child_root;
	unsigned int *parent_root;
	unsigned int child_ttbr0;
	unsigned int n;

	arm_process_roots_init();
	if(arm_process_address_space_create(&processes[0], 0)) {
		return 1;
	}
	parent_root = arm_process_root(&processes[0]);
	if(!parent_root || processes[0].rss != ARM_VM_ROOT_PAGES ||
		arm_vm_map_user_section(parent_root, 0x00100000U,
			0x47000000U, 1)) {
		return 2;
	}
	if(arm_process_address_space_create(&processes[1], &processes[0])) {
		return 3;
	}
	child_root = arm_process_root(&processes[1]);
	if(!child_root || child_root == parent_root ||
		child_root[1] ||
		child_root[ARM_VM_RAM_BASE >> 20] !=
			parent_root[ARM_VM_RAM_BASE >> 20] ||
		arm_vm_map_user_section(child_root, 0x00100000U,
			0x47100000U, 1) ||
		child_root[1] == parent_root[1]) {
		return 4;
	}
	for(n = 2; n < NR_PROCS; n++) {
		if(arm_process_address_space_create(&processes[n], 0)) {
			return 5;
		}
	}
	if(!arm_process_address_space_create(&processes[NR_PROCS], 0)) {
		return 6;
	}
	child_ttbr0 = processes[1].arch.ttbr0;
	if(arm_process_address_space_release(&processes[1]) ||
		processes[1].arch.ttbr0 ||
		processes[1].rss ||
		arm_process_root(&processes[1])) {
		return 7;
	}
	if(arm_process_address_space_create(&processes[1], &processes[0]) ||
		processes[1].arch.ttbr0 != child_ttbr0 ||
		arm_process_root(&processes[1])[1] ||
		arm_process_context_activate(&processes[1])) {
		return 8;
	}

	forged.arch.ttbr0 = processes[0].arch.ttbr0;
	if(arm_process_root(&forged) ||
		arm_process_address_space_release(&forged) != -1 ||
		arm_process_context_activate(&forged) != -1 ||
		arm_process_context_activate(0) != -1) {
		return 9;
	}
	if(!arm_process_address_space_create(&processes[0], 0) ||
		!arm_process_address_space_create(&processes[NR_PROCS],
			&forged)) {
		return 10;
	}
	return 0;
}
