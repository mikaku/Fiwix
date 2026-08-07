/*
 * fiwix/include/fiwix/arm_devices.h
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_ARM_DEVICES_H
#define _FIWIX_ARM_DEVICES_H

#define ARM_PL011_MAJOR		4
#define ARM_PL011_MINOR		64
#define ARM_VIRTIO_BLK_MAJOR	8
#define ARM_VIRTIO_BLK_MINOR	0

#define ARM_GICD_BASE		0x08000000U
#define ARM_GICC_BASE		0x08010000U
#define ARM_PL011_BASE		0x09000000U

int arm_pl011_init(void);
int arm_ext2_writable_gate(void);
int arm_virtio_block_init(void);
int arm_virtio_transport_init(void);
int arm_virtio_read_sector(unsigned long long, void *);
int arm_virtio_write_sector(unsigned long long, void *);
unsigned long long arm_virtio_capacity_sectors(void);
unsigned int arm_virtio_transport_version(void);
void arm_generic_interrupt_init(void);

#endif /* _FIWIX_ARM_DEVICES_H */
