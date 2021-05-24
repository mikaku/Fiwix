/*
 * fiwix/include/fiwix/bios.h
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_BIOS_H
#define _FIWIX_BIOS_H

#include <fiwix/multiboot1.h>

#define NR_BIOS_MM_ENT		50	/* entries in BIOS memory map */

struct bios_mem_map {
	unsigned long int from;
	unsigned long int to;
	int type;
};
struct bios_mem_map bios_mem_map[NR_BIOS_MM_ENT];

int addr_in_bios_map(unsigned int);
void bios_map_add(unsigned long int, unsigned long int, int, int);
void bios_map_init(struct multiboot_mmap_entry *, unsigned long int);

#endif /* _FIWIX_BIOS_H */
