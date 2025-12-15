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
#define PCI_CACHE_LINE_SIZE	0x0C	/*  8 bits */
#define PCI_LATENCY_TIMER	0x0D	/*  8 bits */

#define PCI_HEADER_TYPE		0x0E	/*  8 bits */
#define PCI_HEADER_TYPE_NORMAL	0x00	/* standard header */

#define PCI_BIST		0x0F	/*  8 bits */
#define PCI_BASE_ADDR_0		0x10	/* 32 bits */
#define PCI_BASE_ADDR_1		0x14	/* 32 bits (header 0 and 1 only) */
#define PCI_BASE_ADDR_2		0x18	/* 32 bits (header 0 only) */
#define PCI_BASE_ADDR_3		0x1C	/* 32 bits */
#define PCI_BASE_ADDR_4		0x20	/* 32 bits */
#define PCI_BASE_ADDR_5		0x24	/* 32 bits */

#define PCI_BASE_ADDR_SPACE	0x01	/* 0 = memory, 1 = I/O */
#define PCI_BASE_ADDR_SPACE_MEM	0x00
#define PCI_BASE_ADDR_SPACE_IO	0x01
#define PCI_BASE_ADDR_TYPE_MASK	0x06	/* base register size (32bit/64bit) */
#define PCI_BASE_ADDR_TYPE_32	0x00	/* 32 bit address */
#define PCI_BASE_ADDR_TYPE_64	0x04	/* 64 bit address */
#define PCI_BASE_ADDR_MEM_PREF	0x08	/* prefetchable memory */
#define PCI_BASE_ADDR_MEM_MASK	0xFFFFFFF0
#define PCI_BASE_ADDR_IO_MASK	0xFFFFFFFC

#define PCI_INTERRUPT_LINE	0x3C	/*  8 bits */
#define PCI_INTERRUPT_PIN	0x3D	/*  8 bits */
#define PCI_MIN_GRANT		0x3E	/*  8 bits */
#define PCI_MAX_LATENCY		0x3F	/*  8 bits */

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

/* flags */
#define PCI_F_ADDR_SPACE_IO	0x01
#define PCI_F_ADDR_SPACE_MEM	0x02
#define PCI_F_ADDR_MEM_32	0x04	/* 32 bit address */
#define PCI_F_ADDR_MEM_64	0x08	/* 64 bit address */
#define PCI_F_ADDR_SPACE_PREFET	0x10	/* prefetchable memory */

#define PCI_DEVFN(dev,func)	((((dev) & 0x1f) << 3) | ((func) & 0x07))

struct pci_supported_devices {
	unsigned short int vendor_id;
	unsigned short int device_id;
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
	unsigned char cline_size;
	unsigned char latency;
	unsigned char hdr_type;
	unsigned char bist;
	unsigned int obar[6];	/* original address */
	unsigned int bar[6];
	unsigned char irq;
	unsigned char pin;
	unsigned char min_gnt;
	unsigned char max_lat;
	char size[6];
	char flags[6];
	const char *name;
	struct pci_device *prev;
	struct pci_device *next;
};
extern struct pci_device *pci_device_table;

unsigned char pci_read_char(struct pci_device *, int);
unsigned short int pci_read_short(struct pci_device *, int);
unsigned int pci_read_long(struct pci_device *, int);
void pci_write_char(struct pci_device *, int, unsigned char);
void pci_write_short(struct pci_device *, int, unsigned short int);
void pci_write_long(struct pci_device *, int, unsigned int);
void pci_show_desc(struct pci_device *);
void pci_init(void);

#endif /* _FIWIX_PCI_H */

#endif /* CONFIG_PCI */
