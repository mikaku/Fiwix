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

/* check if an specific address is available in the BIOS memory map */
int addr_in_bios_map(unsigned int addr)
{
	int n;
	struct bios_mem_map *bmm;

	bmm = &bios_mem_map[0];
	for(n = 0; n < NR_BIOS_MM_ENT; n++, bmm++) {
		if(bmm->to && bmm->type == BIOS_MEM_AVAIL) {
			if(addr >= bmm->from && addr < (bmm->to & PAGE_MASK)) {
				return 1;
			}
		}
	}
	return 0;	/* not in BIOS map or not available (reserved, ...) */
}

void bios_map_init(memory_map_t *bmmap_addr, unsigned long int bmmap_length)
{
	unsigned int n;
	unsigned int mem_from, mem_to, len_low, len_high;
	memory_map_t *bmmap;
	char *bios_mem_type[] = { NULL, "available" , "reserved",
				  "ACPI Reclaim", "ACPI NVS", "unusable",
				  "disabled" };

	bmmap = bmmap_addr;
	if(bmmap) {
		n = 0;

		while((unsigned int)bmmap < (unsigned int)bmmap_addr + bmmap_length) {
			mem_from = (unsigned int)bmmap->base_addr_low;
			len_low = (unsigned int)bmmap->length_low;
			if((((mem_from >> 16) & 0xFFFF) + ((len_low >> 16) & 0xFFFF)) > 0xFFFF) {
				mem_to = ~0;
				len_high = (mem_from >> 16) + (len_low >> 16);
				len_high >>= 16;
			} else {
				mem_to = mem_from + len_low;
				len_high = 0;
			}
			printk("%s    0x%08X%08X-0x%08X%08X %s\n",
				n ? "      " : "memory",
				bmmap->base_addr_high, bmmap->base_addr_low,
				len_high, mem_from + len_low,
				bios_mem_type[(int)bmmap->type]);
			/* only memory addresses below 4GB are accepted */
			if(!bmmap->base_addr_high) {
				if(n < NR_BIOS_MM_ENT && len_low) {
					bios_mem_map[n].from = mem_from;
					bios_mem_map[n].to = mem_to;
					bios_mem_map[n].type = (int)bmmap->type;
					n++;
				}
			}
			bmmap = (memory_map_t *)((unsigned int)bmmap + bmmap->size + sizeof(bmmap->size));
		}
	} else {
		printk("WARNING: your BIOS has not provided a memory map.\n");
		bios_mem_map[0].from = 0;
		bios_mem_map[0].to = _memsize * 1024;
		bios_mem_map[0].type = BIOS_MEM_AVAIL;
		bios_mem_map[1].from = 0x00100000;
		bios_mem_map[1].to = (_extmemsize + 1024) * 1024;
		bios_mem_map[1].type = BIOS_MEM_AVAIL;
	}
	kstat.physical_pages = (_extmemsize + 1024) >> 2;

	/*
	 * This truncates to 1GB since it's the maximum physical memory
	 * currently supported.
	 */
	if(kstat.physical_pages > (0x40000000 >> PAGE_SHIFT)) {
		kstat.physical_pages = (0x40000000 >> PAGE_SHIFT);
		printk("WARNING: only up to 1GB of physical memory will be used.\n");
	}
}
