/*
 * fiwix/include/fiwix/arm_vm.h
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_ARM_VM_H
#define _FIWIX_ARM_VM_H

#define ARM_VM_ROOT_ENTRIES		4096U
#define ARM_VM_ROOT_ALIGNMENT		0x00004000U
#define ARM_VM_SECTION_SIZE		0x00100000U
#define ARM_VM_USER_BASE		0x00100000U
#define ARM_VM_USER_LIMIT		0x40000000U
#define ARM_VM_RAM_BASE			0x40000000U
#define ARM_VM_IDENTITY_LIMIT		0x48000000U

#define ARM_VM_SECTION_SUPERVISOR	0x0001140EU
#define ARM_VM_SECTION_USER		0x00011C0EU
#define ARM_VM_SECTION_USER_XN		0x00011C1EU
#define ARM_VM_SECTION_DEVICE_XN	0x00010412U

int arm_vm_root_init(unsigned int *);
int arm_vm_root_clone(unsigned int *, const unsigned int *);
int arm_vm_map_user_section(unsigned int *, unsigned int, unsigned int, int);
int arm_vm_unmap_user_section(unsigned int *, unsigned int);
unsigned int arm_vm_ttbr0(const unsigned int *);
int arm_vm_activate(const unsigned int *);
int arm_vm_context_activate(const unsigned int *);

#endif /* _FIWIX_ARM_VM_H */
