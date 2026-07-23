/* Firmware-free boot hooks for the generic RV64 kernel image. */

#include <fiwix/arch_process.h>
#include <fiwix/asm.h>
#include <fiwix/buffer.h>
#include <fiwix/devices.h>
#include <fiwix/kernel.h>
#include <fiwix/kparms.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/riscv64_devices.h>
#include <fiwix/riscv64_fdt.h>
#include <fiwix/riscv64_trap.h>
#include <fiwix/sched.h>
#include <fiwix/stat.h>
#include <fiwix/string.h>
#include <fiwix/timer.h>
#include <fiwix/tty.h>

#define UART_THR	0
#define UART_LSR	5
#define UART_LSR_THRE	0x20
#define TEST_FAIL	0x00013333U

volatile unsigned long riscv64_timer_ticks;
volatile unsigned long riscv64_user_exit_status;

/* start_kernel() clears BSS after entry, so preserve firmware state in data. */
static unsigned long riscv64_boot_hartid = 1;
static unsigned long riscv64_boot_dtb = 1;

unsigned long riscv64_boot_memory_pages(void)
{
	unsigned long pages;

	pages = riscv64_fdt_memory_pages((const void *)riscv64_boot_dtb,
		PHYSICAL_MEMORY_BASE, RISCV64_MEMORY_LIMIT);
	if(!pages) {
		pages = RISCV64_MEMORY_FALLBACK >> PAGE_SHIFT;
	}
	return pages;
}

extern int riscv64_linux_image_gate(void);
extern unsigned long riscv64_linux_image_entry(void);
extern void riscv64_linux_handoff(unsigned long, unsigned long, unsigned long);

static void riscv64_uart_putc_at(unsigned long address, char c)
{
	volatile unsigned char *uart;

	uart = (volatile unsigned char *)address;
	while(!(uart[UART_LSR] & UART_LSR_THRE)) {
		/* Polling is required before the generic console exists. */
	}
	uart[UART_THR] = (unsigned char)c;
}

static void riscv64_uart_puts_at(unsigned long address, const char *text)
{
	while(*text) {
		if(*text == '\n') {
			riscv64_uart_putc_at(address, '\r');
		}
		riscv64_uart_putc_at(address, *text++);
	}
}

static void riscv64_uart_puts(const char *text)
{
	riscv64_uart_puts_at(riscv64_read_satp() ?
		RISCV64_UART_VIRTUAL_BASE : RISCV64_UART_PHYSICAL_BASE, text);
}

static void riscv64_finish_at(unsigned long address, unsigned int status)
{
	volatile unsigned int *finisher;

	finisher = (volatile unsigned int *)address;
	*finisher = status;
	for(;;) {
		/* QEMU exits through the test device before this loop. */
	}
}

static void riscv64_finish(unsigned int status)
{
	riscv64_finish_at(riscv64_read_satp() ?
		RISCV64_FINISHER_VIRTUAL_BASE :
		RISCV64_FINISHER_PHYSICAL_BASE, status);
}

void riscv64_machine_main(unsigned long hartid, unsigned long dtb)
{
	riscv64_boot_hartid = hartid;
	riscv64_boot_dtb = dtb;
}

void riscv64_supervisor_main(unsigned long hartid, unsigned long dtb)
{
	riscv64_boot_hartid = hartid;
	riscv64_boot_dtb = dtb;
	start_kernel(0, 0, 0);
	riscv64_finish(TEST_FAIL);
}

int riscv64_linux_kexec(void)
{
	unsigned long flags;

	SAVE_FLAGS(flags);
	CLI();
	if(riscv64_linux_image_gate() < 0) {
		riscv64_uart_puts("Fiwix riscv64 Linux Image load failed\n");
		RESTORE_FLAGS(flags);
		return -1;
	}
	riscv64_uart_puts("Fiwix riscv64 Linux Image header gate passed\n");
	riscv64_uart_puts("Fiwix riscv64 Linux root handoff\n");
	riscv64_linux_handoff(riscv64_linux_image_entry(),
		riscv64_boot_hartid, riscv64_boot_dtb);
	return -1;
}

void riscv64_generic_runtime_ready(void)
{
	static const char disk_marker[] =
		"Fiwix riscv64 virtio sector gate\n";
	struct buffer *buffer;
	struct inode *inode;
	__dev_t uart_dev;
	__dev_t block_dev;
	int valid;

	uart_dev = MKDEV(RISCV64_UART_MAJOR, RISCV64_UART_MINOR);
	block_dev = MKDEV(RISCV64_VIRTIO_BLK_MAJOR,
		RISCV64_VIRTIO_BLK_MINOR);
	buffer = bread(block_dev, 0, BLKSIZE_1K);
	valid = buffer && !memcmp(buffer->data, disk_marker,
		sizeof(disk_marker) - 1);
	if(buffer) {
		brelse(buffer);
	}
	inode = NULL;
	if(namei("/bootstrap", &inode, NULL, FOLLOW_LINKS) ||
		!inode || !S_ISREG(inode->i_mode)) {
		valid = 0;
	}
	if(inode) {
		iput(inode);
	}
	if(!kpage_dir || !page_table || !current || current->pid != IDLE ||
		proc_table[INIT].pid != INIT ||
		proc_table[INIT].state != PROC_RUNNING ||
		!proc_table[INIT].arch.satp ||
		!proc_table[INIT].arch.kernel_sp || !kstat.free_pages ||
		!CURRENT_TIME || CURRENT_TICKS < 3 || kparms.rootdev != block_dev ||
		!get_device(CHR_DEV, uart_dev) || !get_tty(uart_dev) ||
		!get_device(BLK_DEV, block_dev) || !valid) {
		riscv64_uart_puts("Fiwix riscv64 generic runtime init failed\n");
		riscv64_finish(TEST_FAIL);
	}
	riscv64_uart_puts("Fiwix riscv64 Goldfish RTC gate passed\n");
	riscv64_uart_puts("Fiwix riscv64 generic PID 1 construction passed\n");
}

void riscv64_trap(unsigned long cause, unsigned long epc,
	unsigned long value)
{
	(void)cause;
	(void)epc;
	(void)value;
	/* M mode does not translate addresses even when satp is nonzero. */
	riscv64_uart_puts_at(RISCV64_UART_PHYSICAL_BASE,
		"Fiwix riscv64 fatal machine trap\n");
	riscv64_finish_at(RISCV64_FINISHER_PHYSICAL_BASE, TEST_FAIL);
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
