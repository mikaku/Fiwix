/*
 * Fixed QEMU virt devices used during riscv64 bring-up.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_RISCV64_DEVICES_H
#define _FIWIX_RISCV64_DEVICES_H

#include <fiwix/fs.h>

#define RISCV64_UART_MAJOR	4
#define RISCV64_UART_MINOR	64
#define RISCV64_VIRTIO_BLK_MAJOR	8
#define RISCV64_VIRTIO_BLK_MINOR	0
#define RISCV64_MMIO_VIRTUAL_BASE	0xffffffc000000000UL
#define RISCV64_FINISHER_PHYSICAL_BASE	0x00100000UL
#define RISCV64_RTC_PHYSICAL_BASE	0x00101000UL
#define RISCV64_PLIC_PHYSICAL_BASE	0x0c000000UL
#define RISCV64_UART_PHYSICAL_BASE	0x10000000UL
#define RISCV64_FINISHER_VIRTUAL_BASE	(RISCV64_MMIO_VIRTUAL_BASE + \
	RISCV64_FINISHER_PHYSICAL_BASE)
#define RISCV64_RTC_VIRTUAL_BASE	(RISCV64_MMIO_VIRTUAL_BASE + \
	RISCV64_RTC_PHYSICAL_BASE)
#define RISCV64_UART_VIRTUAL_BASE	(RISCV64_MMIO_VIRTUAL_BASE + \
	RISCV64_UART_PHYSICAL_BASE)
#define RISCV64_VIRTIO_VIRTUAL_BASE	RISCV64_UART_VIRTUAL_BASE

void riscv64_uart_init(void);
int riscv64_virtio_block_init(void);
int riscv64_virtio_transport_init(void);
int riscv64_virtio_read_sector(unsigned long, void *);
int riscv64_virtio_write_sector(unsigned long, void *);
unsigned long riscv64_virtio_capacity_sectors(void);

#endif /* _FIWIX_RISCV64_DEVICES_H */
