/*
 * fiwix/mm/bios_map.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/mm.h>
#include <fiwix/bios.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct bios_mem_map bios_mem_map[NR_BIOS_MM_ENT];

static char *bios_mem_type[] = {
	NULL,
	"available",
	"reserved",
	"ACPI Reclaim",
	"ACPI NVS",
	"unusable",
	"disabled"
};

static void bios_map_add(unsigned int from, unsigned int to, int from_type, int to_type)
{
	int n;

	for(n = 0; n < NR_BIOS_MM_ENT; n++) {
		if(!bios_mem_map[n].type) {
			if(from_type == to_type) {
				printk("memory    0x%08x%08x-0x%08x%08x %s\n",
					0, from,
					0, to - 1,
					bios_mem_type[to_type]
				);
			} else {
				printk("memory    0x%08x%08x-0x%08x%08x %s -> %s\n",
					0, from,
					0, to - 1,
					bios_mem_type[from_type],
					bios_mem_type[to_type]
				);
			}
			bios_mem_map[n].from = from;
			bios_mem_map[n].to = to;
			bios_mem_map[n].from_hi = 0;
			bios_mem_map[n].to_hi = 0;
			bios_mem_map[n].type = to_type;
			break;
		}
	}

	if(n >= NR_BIOS_MM_ENT) {
		printk("WARNING: %s(): no more entries in bios_mem_map[].\n", __FUNCTION__);
		return;
	}
}

/* check if an specific address is available in the BIOS memory map */
int is_addr_in_bios_map(unsigned int addr)
{
	int n, retval;
	struct bios_mem_map *bmm;

	retval = 0;
	bmm = &bios_mem_map[0];

	for(n = 0; n < NR_BIOS_MM_ENT; n++, bmm++) {
		if(bmm->to && bmm->type == MULTIBOOT_MEMORY_AVAILABLE && !bmm->from_hi && !bmm->to_hi) {
			if(addr >= bmm->from && addr < (bmm->to & PAGE_MASK)) {
				retval = 1;
			}
		}
	}

	/* this second pass is necessary because the array is not sorted */
	bmm = &bios_mem_map[0];
	for(n = 0; n < NR_BIOS_MM_ENT; n++, bmm++) {
		if(bmm->to && bmm->type == MULTIBOOT_MEMORY_RESERVED && !bmm->from_hi && !bmm->to_hi) {
			if(addr >= bmm->from && addr < (bmm->to & PAGE_MASK)) {
				retval = 0;
			}
		}
	}

	return retval;
}

void bios_map_reserve(unsigned int from, unsigned int to)
{
	if(is_addr_in_bios_map(from)) {
		bios_map_add(from, to, MULTIBOOT_MEMORY_AVAILABLE, MULTIBOOT_MEMORY_RESERVED);
		reserve_pages(from, to);
	}
}

void bios_map_init(struct multiboot_mmap_entry *bmmap_addr, unsigned int bmmap_length)
{
	struct multiboot_mmap_entry *bmmap;
	unsigned int from_high, from_low, to_high, to_low;
	unsigned long long to;
	int n, type;

	bmmap = bmmap_addr;
	kstat.physical_pages = 0;

	if(bmmap) {
		n = 0;

		while((unsigned int)bmmap < (unsigned int)bmmap_addr + bmmap_length) {
			from_high = (unsigned int)(bmmap->addr >> 32);
			from_low = (unsigned int)(bmmap->addr & 0xFFFFFFFF);
			to = (bmmap->addr + bmmap->len) - 1;
			to_high = (unsigned int)(to >> 32);
			to_low = (unsigned int)(to & 0xFFFFFFFF);
			type = (int)bmmap->type;
			printk("%s    0x%08x%08x-0x%08x%08x %s\n",
				n ? "      " : "memory",
				from_high,
				from_low,
				to_high,
				to_low,
				bios_mem_type[type]
			);
			if(n < NR_BIOS_MM_ENT && bmmap->len) {
				bios_mem_map[n].from = from_low;
				bios_mem_map[n].from_hi = from_high;
				bios_mem_map[n].to = to_low;
				bios_mem_map[n].to_hi = to_high;
				bios_mem_map[n].type = type;
				/* only memory addresses below 4GB are counted */
				if(!from_high && !to_high) {
					if(type == MULTIBOOT_MEMORY_AVAILABLE) {
						from_low &= PAGE_MASK;
						to_low &= PAGE_MASK;

						/* the first MB is not counted here */
						if(from_low >= 0x100000) {
							kstat.physical_pages += (to_low - from_low) / PAGE_SIZE;
						}
					}
				}
				n++;
			}
			bmmap = (struct multiboot_mmap_entry *)((unsigned int)bmmap + bmmap->size + sizeof(bmmap->size));
		}
		kstat.physical_pages += (1024 >> 2);	/* add the first MB as a whole */
		if(kstat.physical_pages > (GDT_BASE >> PAGE_SHIFT)) {
			printk("WARNING: detected a total of %dMB of available memory below 4GB.\n", (kstat.physical_pages << 2) / 1024);
		}
	} else {
		printk("WARNING: your BIOS has not provided a memory map.\n");
		bios_mem_map[0].from = 0;
		bios_mem_map[0].to = kparm_memsize * 1024;
		bios_mem_map[0].from_hi = 0;
		bios_mem_map[0].to_hi = 0;
		bios_mem_map[0].type = MULTIBOOT_MEMORY_AVAILABLE;
		bios_mem_map[1].from = 0x00100000;
		bios_mem_map[1].to = (kparm_extmemsize + 1024) * 1024;
		bios_mem_map[1].from_hi = 0;
		bios_mem_map[1].to_hi = 0;
		bios_mem_map[1].type = MULTIBOOT_MEMORY_AVAILABLE;
		kstat.physical_pages = (kparm_extmemsize + 1024) >> 2;
	}

	/*
	 * Truncate physical memory to upper kernel address space size (1GB or 2GB), since
	 * currently all memory is permanently mapped there.
	 */
	if(kstat.physical_pages > (GDT_BASE >> PAGE_SHIFT)) {
		kstat.physical_pages = (GDT_BASE >> PAGE_SHIFT);
		printk("WARNING: only up to %dGB of physical memory will be used.\n", GDT_BASE >> 30);
	}
}
