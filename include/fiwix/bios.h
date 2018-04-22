/*
 * fiwix/include/fiwix/bios.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_BIOS_H
#define _FIWIX_BIOS_H

#include <fiwix/multiboot.h>

#define BIOS_MEM_AVAIL		1	/* BIOS memory available */
#define BIOS_MEM_RES		2	/* BIOS memory reserved */
#define BIOS_MEM_ACPI_REC	3	/* BIOS memory ACPI reclaim */
#define BIOS_MEM_ACPI_NVS	4	/* BIOS memory ACPI NVS */
#define NR_BIOS_MM_ENT		25	/* entries in BIOS memory map */

struct bios_mem_map {
	unsigned long int from;
	unsigned long int to;
	unsigned long int type;
};
struct bios_mem_map bios_mem_map[NR_BIOS_MM_ENT];

int addr_in_bios_map(unsigned int);
void bios_map_init(memory_map_t *, unsigned long int);

#endif /* _FIWIX_BIOS_H */
