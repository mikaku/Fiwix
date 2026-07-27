/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <fiwix/arm_vm.h>

static int arm_vm_root_aligned(const unsigned int *root)
{
	return root &&
		!((unsigned long)root & (ARM_VM_ROOT_ALIGNMENT - 1U));
}

static int arm_vm_kernel_device_address(unsigned int address)
{
	unsigned int section;

	section = address >> 20;
	return section == 0x080 || section == 0x081 ||
		section == 0x090 || section == 0x0A0;
}

static int arm_vm_user_address(unsigned int address)
{
	return address >= ARM_VM_USER_BASE &&
		address < ARM_VM_USER_LIMIT &&
		!arm_vm_kernel_device_address(address) &&
		!(address & (ARM_VM_SECTION_SIZE - 1U));
}

int arm_vm_root_init(unsigned int *root)
{
	unsigned int address;
	unsigned int n;

	if(!arm_vm_root_aligned(root)) {
		return -1;
	}
	for(n = 0; n < ARM_VM_ROOT_ENTRIES; n++) {
		root[n] = 0;
	}
	for(address = ARM_VM_RAM_BASE; address < ARM_VM_IDENTITY_LIMIT;
		address += ARM_VM_SECTION_SIZE) {
		root[address >> 20] = address | ARM_VM_SECTION_SUPERVISOR;
	}
	root[0x080] = 0x08000000U | ARM_VM_SECTION_DEVICE_XN;
	root[0x081] = 0x08100000U | ARM_VM_SECTION_DEVICE_XN;
	root[0x090] = 0x09000000U | ARM_VM_SECTION_DEVICE_XN;
	root[0x0A0] = 0x0A000000U | ARM_VM_SECTION_DEVICE_XN;
	return 0;
}

int arm_vm_root_clone(unsigned int *child, const unsigned int *parent)
{
	unsigned int n;

	if(!arm_vm_root_aligned(child) || !arm_vm_root_aligned(parent)) {
		return -1;
	}
	for(n = 0; n < ARM_VM_ROOT_ENTRIES; n++) {
		child[n] = parent[n];
	}
	return 0;
}

int arm_vm_map_user_section(unsigned int *root, unsigned int virtual_address,
	unsigned int physical_address, int executable)
{
	unsigned int descriptor;

	if(!arm_vm_root_aligned(root) ||
		!arm_vm_user_address(virtual_address) ||
		(executable != 0 && executable != 1) ||
		physical_address < ARM_VM_RAM_BASE ||
		physical_address >= ARM_VM_IDENTITY_LIMIT ||
		physical_address & (ARM_VM_SECTION_SIZE - 1U)) {
		return -1;
	}
	descriptor = executable ?
		ARM_VM_SECTION_USER : ARM_VM_SECTION_USER_XN;
	root[virtual_address >> 20] = physical_address | descriptor;
	return 0;
}

int arm_vm_unmap_user_section(unsigned int *root,
	unsigned int virtual_address)
{
	if(!arm_vm_root_aligned(root) ||
		!arm_vm_user_address(virtual_address)) {
		return -1;
	}
	root[virtual_address >> 20] = 0;
	return 0;
}

unsigned int arm_vm_ttbr0(const unsigned int *root)
{
	if(!arm_vm_root_aligned(root)) {
		return 0;
	}
	return (unsigned int)(unsigned long)root;
}

int arm_vm_activate(const unsigned int *root)
{
	unsigned int table_address;

	table_address = arm_vm_ttbr0(root);
	if(!table_address) {
		return -1;
	}
#ifdef __arm__
	{
		unsigned int zero;
		unsigned int domain;
		unsigned int control;

		zero = 0;
		domain = 1;
		__asm__ volatile("dsb");
		__asm__ volatile("mcr p15, 0, %0, c2, c0, 2" : : "r"(zero));
		__asm__ volatile("mcr p15, 0, %0, c2, c0, 0" :
			: "r"(table_address));
		__asm__ volatile("mcr p15, 0, %0, c3, c0, 0" :
			: "r"(domain));
		__asm__ volatile("mcr p15, 0, %0, c8, c7, 0" : : "r"(zero));
		__asm__ volatile("mcr p15, 0, %0, c7, c5, 0" : : "r"(zero));
		__asm__ volatile("mcr p15, 0, %0, c7, c5, 6" : : "r"(zero));
		__asm__ volatile("dsb");
		__asm__ volatile("isb");
		__asm__ volatile("mrc p15, 0, %0, c1, c0, 0" :
			"=r"(control));
		/* D-cache waits for the generic cache-maintenance contract. */
		control |= 0x00801001U;
		__asm__ volatile("mcr p15, 0, %0, c1, c0, 0" :
			: "r"(control));
		__asm__ volatile("isb");
	}
#endif
	return 0;
}

int arm_vm_context_activate(const unsigned int *root)
{
	unsigned int table_address;

	table_address = arm_vm_ttbr0(root);
	if(!table_address) {
		return -1;
	}
#ifdef __arm__
	__asm__ volatile("dsb");
	__asm__ volatile("mcr p15, 0, %0, c2, c0, 0" :
		: "r"(table_address));
	__asm__ volatile("isb");
	table_address = 0;
	__asm__ volatile("mcr p15, 0, %0, c8, c7, 0" :
		: "r"(table_address));
	__asm__ volatile("dsb");
	__asm__ volatile("isb");
#endif
	return 0;
}
