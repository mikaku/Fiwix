/*
 * fiwix/include/fiwix/pci.h
 *
 * Copyright 2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_PCI

#ifndef _FIWIX_PCI_H
#define _FIWIX_PCI_H

#include <fiwix/pci_ids.h>

#define PCI_ADDRESS	0x0CF8
#define PCI_DATA	0x0CFC

#define PCI_MAX_BUS	16	/* 256 buses maximum */
#define PCI_MAX_DEV	32
#define PCI_MAX_FUNC	8

#define PCI_VENDOR_ID		0x00	/* 16 bits */
#define PCI_DEVICE_ID		0x02	/* 16 bits */

#define PCI_COMMAND		0x04	/* 16 bits */
#define PCI_COMMAND_IO		0x1	/* enable response in I/O space */
#define PCI_COMMAND_MEMORY	0x2	/* enable response in memory space */
#define PCI_COMMAND_MASTER	0x4	/* enable bus mastering */

#define PCI_STATUS		0x06	/* 16 bits */
#define PCI_REVISION_ID		0x08	/*  8 bits */
#define PCI_PROG_IF		0x09	/*  8 bits */
#define PCI_CLASS_DEVICE	0x0A	/* 16 bits (class + subclass) */
#define PCI_LATENCY_TIMER	0x0D	/*  8 bits */
#define PCI_HEADER_TYPE		0x0E	/*  8 bits */
#define PCI_BASE_ADDRESS_0	0x10	/* 32 bits */
#define PCI_BASE_ADDRESS_1	0x14	/* 32 bits (header 0 and 1 only) */
#define PCI_BASE_ADDRESS_2	0x18	/* 32 bits (header 0 only) */
#define PCI_BASE_ADDRESS_3	0x1C	/* 32 bits */
#define PCI_BASE_ADDRESS_4	0x20	/* 32 bits */
#define PCI_BASE_ADDRESS_5	0x24	/* 32 bits */
#define PCI_INTERRUPT_LINE	0x3C	/*  8 bits */
#define PCI_INTERRUPT_PIN	0x3D	/*  8 bits */

#define PCI_BASE_ADDR_SPACE	0x01	/* 0 = memory, 1 = I/O */
#define PCI_BASE_ADDR_SPACE_MEM	0x00
#define PCI_BASE_ADDR_SPACE_IO	0x01

/* bus mastering */
#define BM_COMMAND		0x00	/* command register primary */
#define BM_STATUS		0x02	/* status register primary */
#define BM_PRD_ADDRESS		0x04	/* PRD table address primary */

#define BM_COMMAND_START	0x01	/* start */
#define BM_COMMAND_READ		0x08	/* read from disk and write to memory */
#define BM_COMMAND_WRITE	0x00	/* read from memory and write to disk */

#define BM_STATUS_ACTIVE	0x01	/* active */
#define BM_STATUS_ERROR		0x02	/* error */
#define BM_STATUS_INTR		0x04	/* IDE interrupt */
#define BM_STATUS_DRVDMA	0x20	/* drive 0 is DMA capable */
					/* 0x40: drive 1 is DMA capable */
#define BM_STATUS_SIMPLEX	0x80	/* simplex only */

struct pci_supported_devices {
	unsigned short int vendor_id;
	unsigned short int device_id;
	int bars;
};

struct pci_device {
	unsigned char bus;
	unsigned char dev;
	unsigned char func;
	unsigned short int vendor_id;
	unsigned short int device_id;
	unsigned short int command;
	unsigned short int status;
	unsigned char rev;
	unsigned char prog_if;
	unsigned short int class;
	unsigned char header;
	unsigned int bar[6];
	unsigned char irq;
	unsigned char pin;
	unsigned int size[6];
	struct pci_device *prev;
	struct pci_device *next;
};
extern struct pci_device *pci_device_table;

unsigned char pci_read_char(int, int, int, int);
unsigned short int pci_read_short(int, int, int, int);
unsigned int pci_read_long(int, int, int, int);
void pci_write_char(int, int, int, int, unsigned char);
void pci_write_short(int, int, int, int, unsigned short int);
void pci_write_long(int, int, int, int, unsigned int);
unsigned int pci_get_barsize(struct pci_device *, int);
void pci_show_desc(struct pci_device *);
struct pci_device *pci_get_device(unsigned short int, unsigned short int);
void pci_init(void);

#endif /* _FIWIX_PCI_H */

#endif /* CONFIG_PCI */
