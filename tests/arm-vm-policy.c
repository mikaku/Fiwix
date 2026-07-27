#include <fiwix/arch_process.h>
#include <fiwix/arm_vm.h>
#include <fiwix/mm.h>

static unsigned int parent_root[ARM_VM_ROOT_ENTRIES]
	__attribute__((aligned(ARM_VM_ROOT_ALIGNMENT)));
static unsigned int child_root[ARM_VM_ROOT_ENTRIES]
	__attribute__((aligned(ARM_VM_ROOT_ALIGNMENT)));
static unsigned int user_l2[ARM_VM_L2_ENTRIES]
	__attribute__((aligned(ARM_VM_L2_ALIGNMENT)));

int main(void)
{
	struct arch_context context;
	unsigned int n;

	if(PAGE_OFFSET != ARM_VM_USER_LIMIT ||
		PHYSICAL_MEMORY_BASE != ARM_VM_RAM_BASE ||
		MMAP_START >= PAGE_OFFSET ||
		P2V(ARM_VM_RAM_BASE + PAGE_SIZE) !=
			ARM_VM_RAM_BASE + PAGE_SIZE ||
		V2P(ARM_VM_RAM_BASE + PAGE_SIZE) !=
			ARM_VM_RAM_BASE + PAGE_SIZE ||
		PAGE_TO_PHYS(0) != ARM_VM_RAM_BASE ||
		PHYS_TO_PAGE(ARM_VM_RAM_BASE + PAGE_SIZE) != 1) {
		return 1;
	}
	for(n = 0; n < ARM_VM_ROOT_ENTRIES; n++) {
		parent_root[n] = ~0U;
		child_root[n] = ~0U;
	}
	for(n = 0; n < ARM_VM_L2_ENTRIES; n++) {
		user_l2[n] = ~0U;
	}
	if(arm_vm_root_init(parent_root)) {
		return 2;
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
		return 3;
	}
	if(arm_vm_map_user_section(parent_root, 0x00100000U,
			0x47000000U, 1) ||
		arm_vm_map_user_section(parent_root, 0x00200000U,
			0x47100000U, 0)) {
		return 4;
	}
	if(parent_root[1] != (0x47000000U | ARM_VM_SECTION_USER) ||
		parent_root[2] != (0x47100000U | ARM_VM_SECTION_USER_XN)) {
		return 5;
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
		return 6;
	}
	if(arm_vm_l2_init(user_l2) ||
		arm_vm_attach_user_table(parent_root, 0x00400000U,
			0x47E08000U) ||
		arm_vm_map_user_page(user_l2, 0x00400000U,
			0x47400000U, 1, 0) ||
		arm_vm_map_user_page(user_l2, 0x00401000U,
			0x47500000U, 0, 1)) {
		return 7;
	}
	if(parent_root[4] != (0x47E08000U | ARM_VM_COARSE_TABLE) ||
		user_l2[0] != (0x47400000U | ARM_VM_PAGE_USER_RW_XN) ||
		user_l2[1] != (0x47500000U | ARM_VM_PAGE_USER_RO)) {
		return 8;
	}
	if(arm_vm_l2_init(0) != -1 ||
		arm_vm_l2_init(user_l2 + 1) != -1 ||
		!arm_vm_attach_user_table(parent_root, 0x00400000U,
			0x47E08000U) ||
		!arm_vm_attach_user_table(parent_root, 0x00401000U,
			0x47E0C000U) ||
		!arm_vm_attach_user_table(parent_root, 0x08000000U,
			0x47E0C000U) ||
		!arm_vm_attach_user_table(parent_root, 0x00500000U,
			0x47E0C001U) ||
		!arm_vm_map_user_page(user_l2, 0x00400000U,
			0x47600000U, 1, 0) ||
		!arm_vm_map_user_page(user_l2, 0x00400001U,
			0x47600000U, 1, 0) ||
		!arm_vm_map_user_page(user_l2, 0x08000000U,
			0x47600000U, 1, 0) ||
		!arm_vm_map_user_page(user_l2, 0x00402000U,
			0x47600001U, 1, 0) ||
		!arm_vm_map_user_page(user_l2, 0x00402000U,
			ARM_VM_IDENTITY_LIMIT, 1, 0) ||
		!arm_vm_map_user_page(user_l2, 0x00402000U,
			0x47600000U, 2, 0) ||
		!arm_vm_map_user_page(user_l2, 0x00402000U,
			0x47600000U, 1, 2)) {
		return 9;
	}
	if(arm_vm_unmap_user_page(user_l2, 0x00400000U) ||
		user_l2[0] ||
		!arm_vm_unmap_user_page(user_l2 + 1, 0x00401000U) ||
		!arm_vm_unmap_user_page(user_l2, 0)) {
		return 10;
	}
	if(arm_vm_root_clone(child_root, parent_root) ||
		child_root[1] != parent_root[1] ||
		child_root[2] != parent_root[2] ||
		child_root[4] != parent_root[4]) {
		return 11;
	}
	if(arm_vm_unmap_user_section(child_root, 0x00100000U) ||
		child_root[1] || !parent_root[1] ||
		!arm_vm_unmap_user_section(child_root, 0x09000000U) ||
		!arm_vm_unmap_user_section(child_root, ARM_VM_RAM_BASE)) {
		return 12;
	}
	if(arm_vm_root_init(0) != -1 ||
		arm_vm_root_init(parent_root + 1) != -1 ||
		arm_vm_root_clone(child_root + 1, parent_root) != -1 ||
		arm_vm_ttbr0(parent_root + 1) ||
		arm_vm_activate(parent_root + 1) != -1 ||
		arm_vm_context_activate(parent_root + 1) != -1) {
		return 13;
	}
	if(arm_vm_ttbr0(parent_root) !=
		(unsigned int)(unsigned long)parent_root ||
		arm_vm_activate(parent_root) ||
		arm_vm_context_activate(parent_root)) {
		return 14;
	}

	context.ttbr0 = arm_vm_ttbr0(parent_root);
	context.kernel_sp = 0x47E10000U;
	if(context.ttbr0 != (unsigned int)(unsigned long)parent_root ||
		context.kernel_sp != 0x47E10000U) {
		return 15;
	}
	return 0;
}
