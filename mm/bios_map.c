/*
 * fiwix/mm/bios_map.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/mm.h>
#include <fiwix/bios.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static char *bios_mem_type[] = {
	NULL,
	"available",
	"reserved",
	"ACPI Reclaim",
	"ACPI NVS",
	"unusable",
	"disabled"
};

/* check if an specific address is available in the BIOS memory map */
int addr_in_bios_map(unsigned int addr)
{
	int n, retval;
	struct bios_mem_map *bmm;

	retval = 0;
	bmm = &bios_mem_map[0];

	for(n = 0; n < NR_BIOS_MM_ENT; n++, bmm++) {
		if(bmm->to && bmm->type == MULTIBOOT_MEMORY_AVAILABLE) {
			if(addr >= bmm->from && addr < (bmm->to & PAGE_MASK)) {
				retval = 1;
			}
		}
	}

	/* this second pass is necessary because the array is not sorted */
	bmm = &bios_mem_map[0];
	for(n = 0; n < NR_BIOS_MM_ENT; n++, bmm++) {
		if(bmm->to && bmm->type == MULTIBOOT_MEMORY_RESERVED) {
			if(addr >= bmm->from && addr < (bmm->to & PAGE_MASK)) {
				retval = 0;
			}
		}
	}

	return retval;	/* not in BIOS map or not available (reserved, ...) */
}

void bios_map_add(unsigned long int from, unsigned long int to, int from_type, int to_type)
{
	int n;

	for(n = 0; n < NR_BIOS_MM_ENT; n++) {
		if(!bios_mem_map[n].type) {
			break;
		}
	}

	if(from_type == to_type) {
		printk("memory    0x%08X%08X-0x%08X%08X %s\n",
			0, from,
			0, to,
			bios_mem_type[to_type]
		);
	} else {
		printk("memory    0x%08X%08X-0x%08X%08X %s -> %s\n",
			0, from,
			0, to,
			bios_mem_type[from_type],
			bios_mem_type[to_type]
		);
	}
	bios_mem_map[n].from = from;
	bios_mem_map[n].to = to;
	bios_mem_map[n].type = to_type;
}

void bios_map_init(struct multiboot_mmap_entry *bmmap_addr, unsigned long int bmmap_length)
{
	struct multiboot_mmap_entry *bmmap;
	unsigned int from_high, from_low, to_high, to_low;
	unsigned long long to;
	int n;

	bmmap = bmmap_addr;
	kstat.physical_pages = 0;

	if(bmmap) {
		n = 0;

		while((unsigned int)bmmap < (unsigned int)bmmap_addr + bmmap_length) {
			from_high = (unsigned int)(bmmap->addr >> 32);
			from_low = (unsigned int)(bmmap->addr & 0xFFFFFFFF);
			to = bmmap->addr + bmmap->len;
			to_high = (unsigned int)(to >> 32);
			to_low = (unsigned int)(to & 0xFFFFFFFF);
			printk("%s    0x%08X%08X-0x%08X%08X %s\n",
				n ? "      " : "memory",
				from_high,
				from_low,
				to_high,
				to_low,
				bios_mem_type[(int)bmmap->type]
			);
			/* only memory addresses below 4GB are accepted */
			if(!from_high) {
				if(n < NR_BIOS_MM_ENT && bmmap->len) {
					bios_mem_map[n].from = from_low;
					bios_mem_map[n].to = to_low;
					bios_mem_map[n].type = (int)bmmap->type;
					if(bios_mem_map[n].type == MULTIBOOT_MEMORY_AVAILABLE) {
						from_low = bios_mem_map[n].from & PAGE_MASK;
						to_low = bios_mem_map[n].to & PAGE_MASK;
						kstat.physical_pages += (to_low - from_low) / PAGE_SIZE;
					}
					n++;
				}
			}
			bmmap = (struct multiboot_mmap_entry *)((unsigned int)bmmap + bmmap->size + sizeof(bmmap->size));
		}
		if(kstat.physical_pages > (0x40000000 >> PAGE_SHIFT)) {
			printk("WARNING: detected a total of %dMB of available memory below 4GB.\n", (kstat.physical_pages << 2) / 1024);
		}
	} else {
		printk("WARNING: your BIOS has not provided a memory map.\n");
		bios_mem_map[0].from = 0;
		bios_mem_map[0].to = _memsize * 1024;
		bios_mem_map[0].type = MULTIBOOT_MEMORY_AVAILABLE;
		bios_mem_map[1].from = 0x00100000;
		bios_mem_map[1].to = (_extmemsize + 1024) * 1024;
		bios_mem_map[1].type = MULTIBOOT_MEMORY_AVAILABLE;
		kstat.physical_pages = (_extmemsize + 1024) >> 2;
	}

	/*
	 * This truncates to 1GB since it's the maximum physical memory
	 * currently supported.
	 */
	if(kstat.physical_pages > (0x40000000 >> PAGE_SHIFT)) {
		kstat.physical_pages = (0x40000000 >> PAGE_SHIFT);
		printk("WARNING: only up to 1GB of physical memory will be used.\n");
	}
}
