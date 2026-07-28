/*
 * fiwix/arch/arm/generic-boot.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arch_process.h>
#include <fiwix/arm_trap.h>
#include <fiwix/arm_vm.h>
#include <fiwix/kernel.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/timer.h>

#define PL011_BASE	0x09000000U
#define PL011_DR	(*(volatile unsigned int *)(PL011_BASE + 0x00U))
#define PL011_FR	(*(volatile unsigned int *)(PL011_BASE + 0x18U))
#define PL011_FR_TXFF	0x20U

#define GICC_BASE	0x08010000U
#define GICC_IAR	(*(volatile unsigned int *)(GICC_BASE + 0x00CU))
#define GICC_EOIR	(*(volatile unsigned int *)(GICC_BASE + 0x010U))

extern void arm_poweroff(void);

static void arm_early_putc(char ch)
{
	while(PL011_FR & PL011_FR_TXFF) {
	}
	if(ch == '\n') {
		PL011_DR = '\r';
		while(PL011_FR & PL011_FR_TXFF) {
		}
	}
	PL011_DR = (unsigned int)ch;
}

static void arm_early_puts(const char *text)
{
	while(*text) {
		arm_early_putc(*text++);
	}
}

void arm_boot_trace(const char *text)
{
	arm_early_puts(text);
}

unsigned int arm_boot_memory_pages(void)
{
	return ARM_MEMORY_FALLBACK >> PAGE_SHIFT;
}

unsigned int arm_generic_irq_claim(void)
{
	return GICC_IAR;
}

void arm_generic_irq_complete(unsigned int token)
{
	GICC_EOIR = token;
}

void arm_generic_timer_rearm(void)
{
	unsigned int control;
	unsigned int frequency;
	unsigned int ticks;

	__asm__ volatile("mrc p15, 0, %0, c14, c0, 0" :
		"=r"(frequency));
	ticks = frequency / HZ;
	if(!ticks) {
		ticks = 1;
	}
	control = 0;
	__asm__ volatile("mcr p15, 0, %0, c14, c2, 1" :
		: "r"(control));
	__asm__ volatile("mcr p15, 0, %0, c14, c2, 0" :
		: "r"(ticks));
	control = 1;
	__asm__ volatile("mcr p15, 0, %0, c14, c2, 1" :
		: "r"(control));
	__asm__ volatile("isb");
}

void arm_generic_runtime_ready(void)
{
	if(!kpage_dir || !page_table || !current ||
		current->pid != IDLE || !arm_process_root(current) ||
		!current->arch.ttbr0 || !kstat.free_pages) {
		arm_early_puts("Fiwix ARM generic runtime init failed\n");
		arm_poweroff();
	}
	arm_early_puts("Fiwix ARM generic memory and process init passed\n");
	arm_poweroff();
}

void arm_boot_main(void)
{
	arm_early_puts("Fiwix ARM generic kernel entry\n");
	start_kernel(0, 0, 0);
	arm_early_puts("Fiwix ARM generic start_kernel returned\n");
	arm_poweroff();
}
