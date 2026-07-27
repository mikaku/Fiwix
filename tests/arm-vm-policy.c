#include <fiwix/arch_process.h>
#include <fiwix/arm_vm.h>

static unsigned int parent_root[ARM_VM_ROOT_ENTRIES]
	__attribute__((aligned(ARM_VM_ROOT_ALIGNMENT)));
static unsigned int child_root[ARM_VM_ROOT_ENTRIES]
	__attribute__((aligned(ARM_VM_ROOT_ALIGNMENT)));

int main(void)
{
	struct arch_context context;
	unsigned int n;

	for(n = 0; n < ARM_VM_ROOT_ENTRIES; n++) {
		parent_root[n] = ~0U;
		child_root[n] = ~0U;
	}
	if(arm_vm_root_init(parent_root)) {
		return 1;
	}
	if(parent_root[0] || parent_root[ARM_VM_USER_BASE >> 20] ||
		parent_root[ARM_VM_RAM_BASE >> 20] !=
			(ARM_VM_RAM_BASE | ARM_VM_SECTION_SUPERVISOR) ||
		parent_root[0x47F] !=
			(0x47F00000U | ARM_VM_SECTION_SUPERVISOR) ||
		parent_root[0x080] !=
			(0x08000000U | ARM_VM_SECTION_DEVICE_XN) ||
		parent_root[0x090] !=
			(0x09000000U | ARM_VM_SECTION_DEVICE_XN)) {
		return 2;
	}
	if(arm_vm_map_user_section(parent_root, 0x00100000U,
			0x47000000U, 1) ||
		arm_vm_map_user_section(parent_root, 0x00200000U,
			0x47100000U, 0)) {
		return 3;
	}
	if(parent_root[1] != (0x47000000U | ARM_VM_SECTION_USER) ||
		parent_root[2] != (0x47100000U | ARM_VM_SECTION_USER_XN)) {
		return 4;
	}
	if(!arm_vm_map_user_section(parent_root, 0, 0x47000000U, 1) ||
		!arm_vm_map_user_section(parent_root, 0x00101000U,
			0x47000000U, 1) ||
		!arm_vm_map_user_section(parent_root, 0x08000000U,
			0x47000000U, 1) ||
		!arm_vm_map_user_section(parent_root, ARM_VM_USER_LIMIT,
			0x47000000U, 1) ||
		!arm_vm_map_user_section(parent_root, 0x00300000U,
			0x47001000U, 1) ||
		!arm_vm_map_user_section(parent_root, 0x00300000U,
			0x47200000U, 2) ||
		!arm_vm_map_user_section(parent_root, 0x00300000U,
			ARM_VM_IDENTITY_LIMIT, 1)) {
		return 5;
	}
	if(arm_vm_root_clone(child_root, parent_root) ||
		child_root[1] != parent_root[1] ||
		child_root[2] != parent_root[2]) {
		return 6;
	}
	if(arm_vm_unmap_user_section(child_root, 0x00100000U) ||
		child_root[1] || !parent_root[1] ||
		!arm_vm_unmap_user_section(child_root, 0x09000000U) ||
		!arm_vm_unmap_user_section(child_root, ARM_VM_RAM_BASE)) {
		return 7;
	}
	if(arm_vm_root_init(0) != -1 ||
		arm_vm_root_init(parent_root + 1) != -1 ||
		arm_vm_root_clone(child_root + 1, parent_root) != -1 ||
		arm_vm_ttbr0(parent_root + 1) ||
		arm_vm_activate(parent_root + 1) != -1) {
		return 8;
	}
	if(arm_vm_ttbr0(parent_root) !=
		(unsigned int)(unsigned long)parent_root ||
		arm_vm_activate(parent_root)) {
		return 9;
	}

	context.ttbr0 = arm_vm_ttbr0(parent_root);
	context.kernel_sp = 0x47E10000U;
	if(context.ttbr0 != (unsigned int)(unsigned long)parent_root ||
		context.kernel_sp != 0x47E10000U) {
		return 10;
	}
	return 0;
}
