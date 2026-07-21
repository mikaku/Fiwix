/*
 * fiwix/arch/riscv64/main.c
 *
 * Copyright 2026, Fiwix riscv64 contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arch_process.h>

#define UART0_BASE     0x10000000UL
#define UART_THR       0
#define UART_LSR       5
#define UART_LSR_THRE  0x20
#define TEST_FINISHER  0x00100000UL
#define TEST_PASS      0x00005555U
#define TEST_FAIL      0x00013333U

typedef unsigned long u64;
typedef unsigned char u8;

volatile u64 riscv64_timer_ticks;

#define TASK_STACK_WORDS 512

static struct arch_context boot_context;
static struct arch_context task1_context;
static struct arch_context task2_context;
static u64 task1_stack[TASK_STACK_WORDS] __attribute__((aligned(16)));
static u64 task2_stack[TASK_STACK_WORDS] __attribute__((aligned(16)));
static volatile unsigned int task_switches;

extern void riscv64_context_switch(struct arch_context *,
	struct arch_context *);

static void uart_putc(char c)
{
	volatile u8 *uart;

	uart = (volatile u8 *)UART0_BASE;
	while(!(uart[UART_LSR] & UART_LSR_THRE)) {
		/* polling is intentional during the bring-up milestone */
	}
	uart[UART_THR] = (u8)c;
}

static void uart_puts(const char *s)
{
	while(*s) {
		if(*s == '\n') {
			uart_putc('\r');
		}
		uart_putc(*s++);
	}
}

static void uart_puthex(u64 value)
{
	static const char digits[] = "0123456789abcdef";
	int shift;

	for(shift = 60; shift >= 0; shift -= 4) {
		uart_putc(digits[(value >> shift) & 0x0f]);
	}
}

static void finish(unsigned int status)
{
	volatile unsigned int *finisher;

	finisher = (volatile unsigned int *)TEST_FINISHER;
	*finisher = status;
	for(;;) {
		/* QEMU exits before this loop when the test device is present. */
	}
}

static void task2(void)
{
	for(;;) {
		task_switches++;
		riscv64_context_switch(&task2_context, &task1_context);
	}
}

static void task1(void)
{
	int n;

	for(n = 0; n < 3; n++) {
		task_switches++;
		riscv64_context_switch(&task1_context, &task2_context);
	}
	riscv64_context_switch(&task1_context, &boot_context);
	for(;;) {
		/* A resumed, exhausted test task is a port bug. */
	}
}

static void context_switch_gate(void)
{
	task1_context.ra = (u64)task1;
	task1_context.sp = (u64)(task1_stack + TASK_STACK_WORDS);
	task2_context.ra = (u64)task2;
	task2_context.sp = (u64)(task2_stack + TASK_STACK_WORDS);

	riscv64_context_switch(&boot_context, &task1_context);
	if(task_switches != 6) {
		uart_puts("Fiwix riscv64 context-switch gate failed\n");
		finish(TEST_FAIL);
	}
	uart_puts("Fiwix riscv64 context-switch gate passed: 6 switches\n");
}

void riscv64_machine_main(u64 hartid, u64 dtb)
{
	uart_puts("Fiwix riscv64 milestone 1\n");
	uart_puts("hartid 0x");
	uart_puthex(hartid);
	uart_puts(" dtb 0x");
	uart_puthex(dtb);
	uart_puts("\nfirmware-free machine-mode entry passed\n");
}

void riscv64_supervisor_main(u64 hartid, u64 dtb)
{
	(void)hartid;
	(void)dtb;
	uart_puts("Fiwix riscv64 S-mode entry passed\n");
	context_switch_gate();
	while(riscv64_timer_ticks < 3) {
		__asm__ __volatile__("wfi");
	}
	uart_puts("Fiwix riscv64 timer gate passed: 3 ticks\n");
	finish(TEST_PASS);
}

void riscv64_trap(u64 cause, u64 epc, u64 value)
{
	uart_puts("Fiwix riscv64 fatal machine trap\nmcause 0x");
	uart_puthex(cause);
	uart_puts(" mepc 0x");
	uart_puthex(epc);
	uart_puts(" mtval 0x");
	uart_puthex(value);
	uart_putc('\n');
	finish(TEST_FAIL);
}

void riscv64_supervisor_trap(u64 cause, u64 epc, u64 value)
{
	uart_puts("Fiwix riscv64 fatal supervisor trap\nscause 0x");
	uart_puthex(cause);
	uart_puts(" sepc 0x");
	uart_puthex(epc);
	uart_puts(" stval 0x");
	uart_puthex(value);
	uart_putc('\n');
	finish(TEST_FAIL);
}
