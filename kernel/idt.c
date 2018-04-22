/*
 * fiwix/kernel/idt.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/const.h>
#include <fiwix/types.h>
#include <fiwix/segments.h>
#include <fiwix/string.h>

struct gate_desc idt[NR_IDT_ENTRIES];

struct desc_r idtr = {
	sizeof(idt) - 1,
	(unsigned int)idt
};

static void set_idt_entry(int num, __off_t handler, unsigned int flags)
{
	idt[num].gd_looffset = handler & 0x0000FFFF;
	idt[num].gd_selector = KERNEL_CS;
	idt[num].gd_flags = flags << 8;
	idt[num].gd_hioffset = handler >> 16;
}

void idt_init(void)
{
	int n;

	memset_b(idt, NULL, sizeof(idt));
	for(n = 0; n < NR_IDT_ENTRIES; n++) {
		set_idt_entry(n, (__off_t)&unknown_irq, SD_32INTRGATE | SD_PRESENT);
	}

	/* FIXME: must be SD_32TRAPGATE for true multitasking */
	set_idt_entry(0x00, (__off_t)&except0, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x01, (__off_t)&except1, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x02, (__off_t)&except2, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x03, (__off_t)&except3, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x04, (__off_t)&except4, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x05, (__off_t)&except5, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x06, (__off_t)&except6, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x07, (__off_t)&except7, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x08, (__off_t)&except8, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x09, (__off_t)&except9, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x0A, (__off_t)&exceptA, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x0B, (__off_t)&exceptB, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x0C, (__off_t)&exceptC, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x0D, (__off_t)&exceptD, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x0E, (__off_t)&exceptE, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x0F, (__off_t)&exceptF, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x10, (__off_t)&except10, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x11, (__off_t)&except11, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x12, (__off_t)&except12, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x13, (__off_t)&except13, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x14, (__off_t)&except14, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x15, (__off_t)&except15, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x16, (__off_t)&except16, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x17, (__off_t)&except17, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x18, (__off_t)&except18, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x19, (__off_t)&except19, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x1A, (__off_t)&except1A, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x1B, (__off_t)&except1B, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x1C, (__off_t)&except1C, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x1D, (__off_t)&except1D, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x1E, (__off_t)&except1E, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x1F, (__off_t)&except1F, SD_32INTRGATE | SD_PRESENT);

	set_idt_entry(0x20, (__off_t)&irq0, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x21, (__off_t)&irq1, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x22, (__off_t)&irq2, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x23, (__off_t)&irq3, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x24, (__off_t)&irq4, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x25, (__off_t)&irq5, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x26, (__off_t)&irq6, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x27, (__off_t)&irq7, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x28, (__off_t)&irq8, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x29, (__off_t)&irq9, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x2A, (__off_t)&irq10, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x2B, (__off_t)&irq11, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x2C, (__off_t)&irq12, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x2D, (__off_t)&irq13, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x2E, (__off_t)&irq14, SD_32INTRGATE | SD_PRESENT);
	set_idt_entry(0x2F, (__off_t)&irq15, SD_32INTRGATE | SD_PRESENT);

	/* FIXME: must be SD_32TRAPGATE for true multitasking */
	set_idt_entry(0x80, (__off_t)&syscall, SD_32INTRGATE | SD_DPL3 | SD_PRESENT);

	load_idt((unsigned int)&idtr);
}
