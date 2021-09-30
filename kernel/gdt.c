/*
 * fiwix/kernel/gdt.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/types.h>
#include <fiwix/segments.h>
#include <fiwix/process.h>
#include <fiwix/limits.h>
#include <fiwix/string.h>

struct seg_desc gdt[NR_GDT_ENTRIES];

struct desc_r gdtr = {
	sizeof(gdt) - 1,
	(unsigned int)&gdt
};

static void gdt_set_entry(int num, unsigned int base_addr, unsigned int limit, char loflags, char hiflags)
{
	num /= sizeof(struct seg_desc);
	gdt[num].sd_lolimit = limit & 0xFFFF;
	gdt[num].sd_lobase = base_addr & 0xFFFFFF;
	gdt[num].sd_loflags = loflags;
	gdt[num].sd_hilimit = (limit >> 16) & 0x0F;
	gdt[num].sd_hiflags = hiflags;
	gdt[num].sd_hibase = (base_addr >> 24) & 0xFF;
}

void gdt_init(void)
{
	unsigned char loflags;

	gdt_set_entry(0, 0, 0, 0, 0);	/* null descriptor */

	loflags = SD_CODE | SD_CD | SD_DPL0 | SD_PRESENT;
	gdt_set_entry(KERNEL_CS, 0, 0xFFFFFFFF, loflags, SD_OPSIZE32 | SD_PAGE4KB);
	loflags = SD_DATA | SD_CD | SD_DPL0 | SD_PRESENT;
	gdt_set_entry(KERNEL_DS, 0, 0xFFFFFFFF, loflags, SD_OPSIZE32 | SD_PAGE4KB);

	loflags = SD_CODE | SD_CD | SD_DPL3 | SD_PRESENT;
	gdt_set_entry(USER_CS, 0, 0xFFFFFFFF, loflags, SD_OPSIZE32 | SD_PAGE4KB);
	loflags = SD_DATA | SD_CD | SD_DPL3 | SD_PRESENT;
	gdt_set_entry(USER_DS, 0, 0xFFFFFFFF, loflags, SD_OPSIZE32 | SD_PAGE4KB);

	loflags = SD_TSSPRESENT;
	gdt_set_entry(TSS, 0, sizeof(struct proc) - 1, loflags, SD_OPSIZE32);

	load_gdt((unsigned int)&gdtr);
}
