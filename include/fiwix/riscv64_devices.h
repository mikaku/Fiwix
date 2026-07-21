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

void riscv64_uart_init(void);
int riscv64_virtio_block_init(void);
int riscv64_virtio_transport_init(void);
int riscv64_virtio_read_sector(unsigned long, void *);
unsigned long riscv64_virtio_capacity_sectors(void);

#endif /* _FIWIX_RISCV64_DEVICES_H */
