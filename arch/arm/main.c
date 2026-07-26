/* SPDX-License-Identifier: GPL-2.0-or-later */

typedef unsigned int u32;

#define PL011_BASE 0x09000000U
#define PL011_DR   (*(volatile u32 *)(PL011_BASE + 0x00U))
#define PL011_FR   (*(volatile u32 *)(PL011_BASE + 0x18U))
#define PL011_FR_TXFF 0x20U

extern u32 arm_boot_dtb;
extern void arm_poweroff(void);

static void uart_putc(int c)
{
	while(PL011_FR & PL011_FR_TXFF) {
	}
	PL011_DR = (u32)c;
}

static void uart_puts(const char *s)
{
	while(*s) {
		uart_putc(*s++);
	}
}

static void uart_puthex32(u32 value)
{
	static const char digits[] = "0123456789ABCDEF";
	int shift;

	for(shift = 28; shift >= 0; shift -= 4) {
		uart_putc(digits[(value >> shift) & 0x0FU]);
	}
}

static u32 read_cpsr(void)
{
	u32 value;

	__asm__ volatile("mrs %0, cpsr" : "=r"(value));
	return value;
}

static u32 read_midr(void)
{
	u32 value;

	__asm__ volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(value));
	return value;
}

void arm_boot_main(void)
{
	uart_puts("Fiwix ARMv7 firmware-free boot\n");
	uart_puts("mode=0x");
	uart_puthex32(read_cpsr() & 0x1FU);
	uart_puts(" midr=0x");
	uart_puthex32(read_midr());
	uart_puts(" dtb=0x");
	uart_puthex32(arm_boot_dtb);
	uart_putc('\n');
	uart_puts("arm boot smoke passed\n");
	arm_poweroff();
}
