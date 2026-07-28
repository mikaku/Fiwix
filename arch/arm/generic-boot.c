/*
 * fiwix/arch/arm/generic-boot.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arch_process.h>
#include <fiwix/arm_devices.h>
#include <fiwix/arm_fdt.h>
#include <fiwix/arm_linux.h>
#include <fiwix/arm_trap.h>
#include <fiwix/arm_vm.h>
#include <fiwix/asm.h>
#include <fiwix/buffer.h>
#include <fiwix/devices.h>
#include <fiwix/kernel.h>
#include <fiwix/kparms.h>
#include <fiwix/mm.h>
#include <fiwix/process.h>
#include <fiwix/sched.h>
#include <fiwix/string.h>
#include <fiwix/timer.h>
#include <fiwix/tty.h>

#define PL011_DR	(*(volatile unsigned int *)(unsigned long)\
	(arm_vm_device_address(ARM_PL011_BASE) + 0x00U))
#define PL011_FR	(*(volatile unsigned int *)(unsigned long)\
	(arm_vm_device_address(ARM_PL011_BASE) + 0x18U))
#define PL011_FR_TXFF	0x20U

#define GICD_REGISTER(offset)	(*(volatile unsigned int *)(unsigned long)\
	(arm_vm_device_address(ARM_GICD_BASE) + (offset)))
#define GICC_REGISTER(offset)	(*(volatile unsigned int *)(unsigned long)\
	(arm_vm_device_address(ARM_GICC_BASE) + (offset)))
#define GICD_CTLR	GICD_REGISTER(0x000U)
#define GICD_ISENABLER0	GICD_REGISTER(0x100U)
#define GICD_ICENABLER0	GICD_REGISTER(0x180U)
#define GICD_ICPENDR0	GICD_REGISTER(0x280U)
#define GICD_IPRIORITYR7	GICD_REGISTER(0x41CU)
#define GICC_CTLR	GICC_REGISTER(0x000U)
#define GICC_PMR	GICC_REGISTER(0x004U)
#define GICC_BPR	GICC_REGISTER(0x008U)
#define GICC_IAR	GICC_REGISTER(0x00CU)
#define GICC_EOIR	GICC_REGISTER(0x010U)

extern void arm_poweroff(void);
extern unsigned int arm_boot_dtb;
extern void arm_linux_handoff(unsigned int, unsigned int);

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

int arm_linux_kexec(void)
{
	unsigned int flags;

	SAVE_FLAGS(flags);
	CLI();
	if(arm_linux_prepare() < 0) {
		arm_early_puts("Fiwix ARM Linux zImage load failed\n");
		RESTORE_FLAGS(flags);
		return -1;
	}
	arm_early_puts("Fiwix ARM Linux zImage header gate passed\n");
	arm_early_puts("Fiwix ARM Linux ext2 root handoff\n");
	arm_linux_handoff(arm_linux_image_entry(), arm_linux_dtb_entry());
	return -1;
}

unsigned int arm_boot_memory_pages(void)
{
	unsigned int pages;

	pages = arm_fdt_boot_discover((const void *)arm_boot_dtb,
		PHYSICAL_MEMORY_BASE, ARM_MEMORY_LIMIT);
	return pages ? pages : ARM_MEMORY_FALLBACK >> PAGE_SHIFT;
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
	unsigned int frequency;
	unsigned int ticks;

	frequency = arm_generic_timer_frequency_read();
	ticks = frequency / HZ;
	if(!ticks) {
		ticks = 1;
	}
	arm_generic_timer_program(ticks);
}

void arm_generic_interrupt_init(void)
{
	GICD_CTLR = 0;
	GICC_CTLR = 0;
	GICD_ICENABLER0 = 0xFFFFFFFFU;
	GICD_ICPENDR0 = 0xFFFFFFFFU;
	GICD_IPRIORITYR7 = 0x80808080U;
	GICD_ISENABLER0 = 1U << ARM_PHYS_TIMER_IRQ;
	GICC_BPR = 0;
	GICC_PMR = 0xFFU;
	GICC_CTLR = 1;
	GICD_CTLR = 1;
	arm_data_sync_barrier();
	arm_generic_timer_rearm();
}

void arm_generic_runtime_ready(void)
{
	static const char disk_marker[] =
		"Fiwix ARM virtio sector gate\n";
	const struct arm_fdt_info *fdt;
	struct buffer *buffer;
	__dev_t block;
	__dev_t console;
	__dev_t serial;
	unsigned int dtb_page;
	unsigned int transport_version;
	int dtb_selected;
	int disk_valid;

	block = MKDEV(ARM_VIRTIO_BLK_MAJOR, ARM_VIRTIO_BLK_MINOR);
	console = MKDEV(SYSCON_MAJOR, 1);
	serial = MKDEV(ARM_PL011_MAJOR, ARM_PL011_MINOR);
	fdt = arm_boot_fdt_info();
	dtb_page = arm_boot_dtb & PAGE_MASK;
	dtb_selected = arm_boot_fdt_selected();
	transport_version = arm_virtio_transport_version();
	buffer = bread(block, 0, BLKSIZE_1K);
	disk_valid = buffer && !memcmp(buffer->data, disk_marker,
		sizeof(disk_marker) - 1);
	if(buffer) {
		brelse(buffer);
	}
	if(!kpage_dir || !page_table || !current ||
		current->pid != IDLE || !arm_process_root(current) ||
		!current->arch.ttbr0 || !kstat.free_pages ||
		proc_table[INIT].pid != INIT ||
		proc_table[INIT].state != PROC_RUNNING ||
		!proc_table[INIT].arch.ttbr0 ||
		!proc_table[INIT].arch.kernel_sp ||
		!get_device(CHR_DEV, console) ||
		!get_device(CHR_DEV, serial) || !get_tty(serial) ||
		!get_device(BLK_DEV, block) || kparms.rootdev != block ||
		!current->root || !current->pwd || !disk_valid ||
		(transport_version != 1 && transport_version != 2) ||
		(fdt && !fdt->virtio_count) ||
		(dtb_selected && (!arm_boot_page_reserved(dtb_page) ||
		!(page_table[PHYS_TO_PAGE(dtb_page)].flags & PAGE_RESERVED))) ||
		CURRENT_TICKS < 3) {
		arm_early_puts("Fiwix ARM generic runtime init failed\n");
		arm_poweroff();
	}
	if(fdt) {
		arm_early_puts("Fiwix ARM firmware DTB discovery passed\n");
	}
	if(transport_version == 1) {
		arm_early_puts("Fiwix ARM virtio-mmio v1 block passed\n");
	} else {
		arm_early_puts("Fiwix ARM virtio-mmio v2 block passed\n");
	}
	arm_early_puts("Fiwix ARM generic console, timer, memory, and "
		"process init passed\n");
	arm_early_puts("Fiwix ARM generic PID 1 construction passed\n");
}

void arm_boot_main(void)
{
	arm_early_puts("Fiwix ARM generic kernel entry\n");
	start_kernel(0, 0, 0);
	arm_early_puts("Fiwix ARM generic start_kernel returned\n");
	arm_poweroff();
}
