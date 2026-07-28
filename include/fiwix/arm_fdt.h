/*
 * fiwix/include/fiwix/arm_fdt.h
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_ARM_FDT_H
#define _FIWIX_ARM_FDT_H

#define ARM_FDT_MAX_VIRTIO_REGIONS	32

struct arm_fdt_region {
	unsigned int address;
	unsigned int size;
};

struct arm_fdt_info {
	unsigned int memory_pages;
	unsigned int virtio_count;
	struct arm_fdt_region virtio[ARM_FDT_MAX_VIRTIO_REGIONS];
};

unsigned int arm_fdt_size(const void *);
int arm_fdt_parse(const void *, unsigned int, unsigned int,
	struct arm_fdt_info *);
unsigned int arm_fdt_boot_discover(const void *, unsigned int, unsigned int);
const struct arm_fdt_info *arm_boot_fdt_info(void);
void arm_fdt_set_boot_blob(const void *, unsigned long, unsigned long);
int arm_boot_fdt_selected(void);
int arm_boot_page_reserved(unsigned long);

#endif /* _FIWIX_ARM_FDT_H */
