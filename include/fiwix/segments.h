/*
 * fiwix/include/fiwix/segments.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_SEGMENTS_H
#define _FIWIX_SEGMENTS_H

#include <fiwix/types.h>

#define NR_GDT_ENTRIES	6	/* entries in GDT descriptor */
#define NR_IDT_ENTRIES	256	/* entries in IDT descriptor */

/* low flags of Segment Descriptors */
#define SD_DATA		0x02	/* DATA Read/Write */
#define SD_CODE		0x0A	/* CODE Exec/Read */

#define SD_32TSSA	0x09	/* 32-bit TSS (Available) */
#define SD_32TSSB	0x0B	/* 32-bit TSS (Busy) */
#define SD_32CALLGATE	0x0C	/* 32-bit Call Gate */
#define SD_32INTRGATE	0x0E	/* 32-bit Interrupt Gate (0D110) */
#define SD_32TRAPGATE	0x0F	/* 32-bit Trap Gate (0D111) */

#define SD_CD 		0x10	/* 0 = system / 1 = code/data */
#define SD_DPL0		0x00	/* priority level 0 */
#define SD_DPL1		0x20	/* priority level 1 (unused) */
#define SD_DPL2		0x40	/* priority level 2 (unused) */
#define SD_DPL3		0x60	/* priority level 3 (user) */
#define SD_PRESENT	0x80	/* segment present or valid */

/* high flags Segment Descriptors */
#define SD_OPSIZE32	0x04	/* 32-bit code and data segments */
#define SD_PAGE4KB	0x08	/* page granularity (4KB) */

/* low flags of the TSS Descriptors */
#define SD_TSSPRESENT	0x89	/* TSS present and not busy flag */

#define USR_PL		3	/* User Privilege Level */

/* EFLAGS */
#define EF_IOPL		12	/* IOPL bit */

struct desc_r {
	__u16 limit;
	__u32 base_addr;
} __attribute__((packed));

struct seg_desc {
	unsigned sd_lolimit : 16;	/* segment limit 0-15 bits */
	unsigned sd_lobase  : 24;	/* base address  0-23 bits */
	unsigned sd_loflags :  8;	/* flags (P, DPL, S and TYPE) */
	unsigned sd_hilimit :  4;	/* segment limit 16-19 bits */
	unsigned sd_hiflags :  4;	/* flags (G, DB, 0 and AVL) */
	unsigned sd_hibase  :  8;	/* base address 24-31 bits */
} __attribute__((packed));

struct gate_desc {
	unsigned gd_looffset: 16;	/* offset 0-15 bits */
	unsigned gd_selector: 16;	/* segment selector */
	unsigned gd_flags   : 16;	/* flags (P, DPL, TYPE, 0 and NULL) */
	unsigned gd_hioffset: 16;	/* offset 16-31 bits */
} __attribute__((packed));

void gdt_init(void);
void idt_init(void);

#endif /* _FIWIX_SEGMENTS_H */
