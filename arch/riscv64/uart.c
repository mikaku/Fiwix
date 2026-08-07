/*
 * Polled 16550 console for the fixed QEMU virt machine.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/charq.h>
#include <fiwix/console.h>
#include <fiwix/devices.h>
#include <fiwix/fs.h>
#include <fiwix/kparms.h>
#include <fiwix/riscv64_devices.h>
#include <fiwix/serial.h>
#include <fiwix/sleep.h>
#include <fiwix/sysconsole.h>
#include <fiwix/tty.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define UART0_BASE	RISCV64_UART_VIRTUAL_BASE

static struct fs_operations riscv64_uart_fsop = {
	0,
	0,

	tty_open,
	tty_close,
	tty_read,
	tty_write,
	tty_ioctl,
	tty_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	tty_select,

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lookup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct device riscv64_uart_device = {
	"ttyS",
	RISCV64_UART_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,
	NULL,
	&riscv64_uart_fsop,
	NULL,
	NULL,
	NULL
};

static struct device riscv64_console_device = {
	"console",
	SYSCON_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	NULL,
	NULL,
	&riscv64_uart_fsop,
	NULL,
	NULL,
	NULL
};

static void riscv64_uart_output(struct tty *tty)
{
	volatile unsigned char *uart;
	unsigned char ch;

	uart = (volatile unsigned char *)UART0_BASE;
	while(tty->write_q.count) {
		while(!(uart[UART_LSR] & UART_LSR_THRE)) {
			/* Bootstrap output is deliberately polled. */
		}
		ch = charq_getchar(&tty->write_q);
		uart[UART_TD] = ch;
	}
	wakeup(&tty_write);
}

void riscv64_uart_init(void)
{
	struct tty *tty;
	__dev_t dev;

	dev = MKDEV(RISCV64_UART_MAJOR, RISCV64_UART_MINOR);
	SET_MINOR(riscv64_uart_device.minors, RISCV64_UART_MINOR);
	SET_MINOR(riscv64_console_device.minors, 0);
	SET_MINOR(riscv64_console_device.minors, 1);
	if(register_device(CHR_DEV, &riscv64_uart_device) ||
		register_device(CHR_DEV, &riscv64_console_device)) {
		return;
	}
	if(!(tty = register_tty(dev))) {
		return;
	}
	tty->deltab = tty_deltab;
	tty->reset = tty_reset;
	tty->input = do_cook;
	tty->output = riscv64_uart_output;
	kparms.syscondev = dev;
	add_sysconsoledev(dev);
	register_console(tty);
	flush_log_buf(tty);
}
