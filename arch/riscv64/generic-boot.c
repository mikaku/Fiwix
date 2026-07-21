/* Firmware-free boot hooks for the generic RV64 kernel image. */

#include <fiwix/arch_process.h>
#include <fiwix/kernel.h>
#include <fiwix/riscv64_trap.h>

#define UART0_BASE	0x10000000UL
#define UART_THR	0
#define UART_LSR	5
#define UART_LSR_THRE	0x20
#define TEST_FINISHER	0x00100000UL
#define TEST_PASS	0x00005555U
#define TEST_FAIL	0x00013333U

volatile unsigned long riscv64_timer_ticks;
volatile unsigned long riscv64_user_exit_status;

static void riscv64_uart_putc(char c)
{
	volatile unsigned char *uart;

	uart = (volatile unsigned char *)UART0_BASE;
	while(!(uart[UART_LSR] & UART_LSR_THRE)) {
		/* Polling is required before the generic console exists. */
	}
	uart[UART_THR] = (unsigned char)c;
}

static void riscv64_uart_puts(const char *text)
{
	while(*text) {
		if(*text == '\n') {
			riscv64_uart_putc('\r');
		}
		riscv64_uart_putc(*text++);
	}
}

static void riscv64_finish(unsigned int status)
{
	volatile unsigned int *finisher;

	finisher = (volatile unsigned int *)TEST_FINISHER;
	*finisher = status;
	for(;;) {
		/* QEMU exits through the test device before this loop. */
	}
}

void riscv64_machine_main(unsigned long hartid, unsigned long dtb)
{
	(void)hartid;
	(void)dtb;
}

void riscv64_supervisor_main(unsigned long hartid, unsigned long dtb)
{
	(void)hartid;
	(void)dtb;
	start_kernel(0, 0, 0);
	riscv64_finish(TEST_FAIL);
}

void riscv64_generic_boot_ready(void)
{
	riscv64_uart_puts("Fiwix riscv64 generic kernel entry passed\n");
	riscv64_finish(TEST_PASS);
}

void riscv64_trap(unsigned long cause, unsigned long epc,
	unsigned long value)
{
	(void)cause;
	(void)epc;
	(void)value;
	riscv64_uart_puts("Fiwix riscv64 fatal machine trap\n");
	riscv64_finish(TEST_FAIL);
}

void riscv64_supervisor_trap(unsigned long cause, unsigned long epc,
	unsigned long value)
{
	(void)cause;
	(void)epc;
	(void)value;
	riscv64_uart_puts("Fiwix riscv64 fatal supervisor trap\n");
	riscv64_finish(TEST_FAIL);
}

unsigned long riscv64_user_trap(struct riscv64_trap_frame *frame,
	unsigned long cause)
{
	return (unsigned long)riscv64_generic_user_trap(frame, cause);
}
