/*
 * fiwix/kernel/idt.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/types.h>
#include <fiwix/segments.h>
#include <fiwix/string.h>

struct gate_desc idt[NR_IDT_ENTRIES];

struct desc_r idtr = {
	sizeof(idt) - 1,
	(unsigned int)idt
};

static void *except_handlers[] = {
	&except0, &except1, &except2, &except3, &except4, &except5, &except6, &except7,
	&except8, &except9, &except10, &except11, &except12, &except13, &except14, &except15,
	&except16, &except17, &except18, &except19, &except20, &except21, &except22, &except23,
	&except24, &except25, &except26, &except27, &except28, &except29, &except30, &except31
};

static void *irq_handlers[] = {
	&irq0, &irq1, &irq2, &irq3, &irq4, &irq5, &irq6, &irq7,
	&irq8, &irq9, &irq10, &irq11, &irq12, &irq13, &irq14, &irq15
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
		if(n < 0x20) {
			/* FIXME: must be SD_32TRAPGATE for true multitasking */
			set_idt_entry(n, (__off_t)except_handlers[n], SD_32INTRGATE | SD_PRESENT);
			continue;
		}
		if(n < 0x30) {
			set_idt_entry(n, (__off_t)irq_handlers[n - 0x20], SD_32INTRGATE | SD_PRESENT);
			continue;
		}
		set_idt_entry(n, (__off_t)&unknown_irq, SD_32INTRGATE | SD_PRESENT);
	}

	/* FIXME: must be SD_32TRAPGATE for true multitasking */
	set_idt_entry(0x80, (__off_t)&syscall, SD_32INTRGATE | SD_DPL3 | SD_PRESENT);

	load_idt((unsigned int)&idtr);
}
