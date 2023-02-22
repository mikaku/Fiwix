/*
 * fiwix/drivers/pci/pci.c
 *
 * Copyright 2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/config.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include <fiwix/pci.h>
#include <fiwix/mm.h>

#ifdef CONFIG_PCI

struct pci_device *pci_device_table = NULL;

/* supported device classes only */
static const char *pci_get_strclass(unsigned short int class)
{
	switch(class) {
		case PCI_CLASS_STORAGE_IDE:		return "IDE interface";
		case PCI_CLASS_DISPLAY_VGA:		return "VGA Display controller";
		case PCI_CLASS_COMMUNICATION_SERIAL:	return "Serial controller";
	}
	return NULL;
}

static const char *pci_get_strvendor_id(unsigned short int vendor_id)
{
#ifdef CONFIG_PCI_NAMES
	switch(vendor_id) {
		case PCI_VENDOR_ID_BOCHS:		return "QEMU";
		case PCI_VENDOR_ID_REDHAT:		return "Red Hat, Inc.";
		case PCI_VENDOR_ID_INTEL:		return "Intel Corporation";
	}
#endif /* CONFIG_PCI_NAMES */
	return NULL;
}

static const char *pci_get_strdevice_id(unsigned short int device_id)
{
#ifdef CONFIG_PCI_NAMES
	switch(device_id) {
		case PCI_DEVICE_ID_BGA:			return "Bochs Graphics Adapter";
		case PCI_DEVICE_ID_QEMU_16550A:		return "QEMU 16550A UART";
		case PCI_DEVICE_ID_INTEL_82371SB_1:	return "82371SB PIIX3 IDE";
	}
#endif /* CONFIG_PCI_NAMES */
	return NULL;
}

static unsigned int get_addr(int bus, int dev, int func, int offset)
{
	unsigned int addr;

	addr = (unsigned int)(0x80000000 |
		(bus << 16) |
		(dev << 11) |
		(func << 8) |
		(offset & 0xFC));

	return addr;
}

static int is_mechanism_1_supported(void)
{
	unsigned int orig, value;

	orig = inport_l(PCI_ADDRESS);

	outport_l(PCI_ADDRESS, 0x80000000);
	value = inport_l(PCI_ADDRESS);
	if(value == 0x80000000) {
		return 1;
	}

	outport_l(PCI_ADDRESS, orig);
	return 0;
}

static void add_pci_device(int bus, int dev, int func, struct pci_device *pci_dev)
{
	unsigned long int flags;
	struct pci_device *pdt;

	if(!(pdt = (struct pci_device *)kmalloc2(sizeof(struct pci_device)))) {
		return;
	}
	memset_b(pdt, 0, sizeof(struct pci_device));

	pci_dev->command = pci_read_short(bus, dev, func, PCI_COMMAND);
	pci_dev->status = pci_read_short(bus, dev, func, PCI_STATUS);
	pci_dev->rev = pci_read_char(bus, dev, func, PCI_REVISION_ID);
	pci_dev->prog_if = pci_read_char(bus, dev, func, PCI_PROG_IF);
	/* BARs are special, they must be handled by the driver */
	pci_dev->pin = pci_read_char(bus, dev, func, PCI_INTERRUPT_PIN);
	*pdt = *pci_dev;

	SAVE_FLAGS(flags); CLI();
	if(!pci_device_table) {
		pci_device_table = pdt;
	} else {
		pdt->prev = pci_device_table->prev;
		pci_device_table->prev->next = pdt;
	}
	pci_device_table->prev = pdt;
	RESTORE_FLAGS(flags);
}

static void scan_bus(void)
{
	int b, d, f;
	unsigned int vendor_id, device_id, class;
	unsigned char header, irq, prog_if;
	struct pci_device pci_dev;
	const char *name;

	for(b = 0; b < PCI_MAX_BUS; b++) {
		for(d = 0; d < PCI_MAX_DEV; d++) {
			for(f = 0; f < PCI_MAX_FUNC; f++) {
				vendor_id = pci_read_short(b, d, f, PCI_VENDOR_ID);
				if(!vendor_id || vendor_id == 0xFFFF) {
					continue;
				}

				device_id = pci_read_short(b, d, f, PCI_DEVICE_ID);
				prog_if = pci_read_char(b, d, f, PCI_PROG_IF);
				class = pci_read_short(b, d, f, PCI_CLASS_DEVICE);
				header = pci_read_char(b, d, f, PCI_HEADER_TYPE);
				irq = pci_read_char(b, d, f, PCI_INTERRUPT_LINE);

				printk("\t  b:%02x d:%02x f:%d", b, d, f);
				if(irq) {
					printk("   %3d", irq);
				} else {
					printk("     -");
				}
				printk("\t%04x:%04x %04x%02x", vendor_id, device_id, class, (int)prog_if);

				name = pci_get_strclass(class);
				if(!name) {
					printk(" - %s\n", "Unknown");
				} else {
					printk(" - %s\n", name);
					pci_dev.bus = b;
					pci_dev.dev = d;
					pci_dev.func = f;
					pci_dev.vendor_id = vendor_id;
					pci_dev.device_id = device_id;
					pci_dev.class = class;
					pci_dev.header = header;
					pci_dev.irq = irq;
					add_pci_device(b, d, f, &pci_dev);
				}
				if(!f && !(header & 0x80)) {
					break;	/* no more functions in this device */
				}
			}
		}
	}
}

unsigned char pci_read_char(int bus, int dev, int func, int offset)
{
	unsigned char retval;

	outport_l(PCI_ADDRESS, get_addr(bus, dev, func, offset));
	retval = inport_l(PCI_DATA) >> ((offset & 3) * 8) & 0xFF;
	return retval;
}

unsigned short int pci_read_short(int bus, int dev, int func, int offset)
{
	unsigned short int retval;

	outport_l(PCI_ADDRESS, get_addr(bus, dev, func, offset));
	retval = inport_l(PCI_DATA) >> ((offset & 2) * 8) & 0xFFFF;
	return retval;
}

unsigned int pci_read_long(int bus, int dev, int func, int offset)
{
	unsigned int retval;

	outport_l(PCI_ADDRESS, get_addr(bus, dev, func, offset));
	retval = inport_l(PCI_DATA) >> ((offset & 2) * 8);
	return retval;
}

void pci_write_char(int bus, int dev, int func, int offset, unsigned char buf)
{
	outport_l(PCI_ADDRESS, get_addr(bus, dev, func, offset));
	outport_b(PCI_DATA, buf);
}

void pci_write_short(int bus, int dev, int func, int offset, unsigned short int buf)
{
	outport_l(PCI_ADDRESS, get_addr(bus, dev, func, offset));
	outport_w(PCI_DATA, buf);
}

void pci_write_long(int bus, int dev, int func, int offset, unsigned int buf)
{
	outport_l(PCI_ADDRESS, get_addr(bus, dev, func, offset));
	outport_l(PCI_DATA, buf);
}

unsigned int pci_get_barsize(int bus, int dev, int func, int bar)
{
	unsigned int tmp, retval;

	/* backup original value */
	tmp = pci_read_long(bus, dev, func, PCI_BASE_ADDRESS_0 + (bar * 4));

	pci_write_long(bus, dev, func, PCI_BASE_ADDRESS_0 + (bar * 4), ~0);
	retval = pci_read_long(bus, dev, func, PCI_BASE_ADDRESS_0 + (bar * 4));
	retval = (~retval) + 1;

	/* restore original value */
	pci_write_long(bus, dev, func, PCI_BASE_ADDRESS_0 + (bar * 4), tmp);

	return retval;
}

void pci_show_desc(struct pci_device *pci_dev)
{
	const char *name;

	printk("\t\t\t\tpci=%02x:%02x.%d rev=%02d\n", pci_dev->bus, pci_dev->dev, pci_dev->func, pci_dev->rev);
	printk("\t\t\t\t");
	name = pci_get_strdevice_id(pci_dev->device_id);
	printk("desc=%s ", name);
	name = pci_get_strvendor_id(pci_dev->vendor_id);
	printk("(%s)\n", name);
}

struct pci_device *pci_get_device(unsigned short int vendor_id, unsigned short int device_id)
{
	struct pci_device *pdt;

	pdt = pci_device_table;

	while(pdt) {
		if(pdt->vendor_id == vendor_id && pdt->device_id == device_id) {
			return pdt;
		}
		pdt = pdt->next;
	}

	return NULL;
}

void pci_init(void)
{
	if(!is_mechanism_1_supported()) {
		return;
	}

	printk("pci       0x%04x-0x%04x", PCI_ADDRESS, PCI_DATA + sizeof(unsigned int) - 1);
	printk("     -\tscanning %d buses, configuration type=1\n", PCI_MAX_BUS);
	scan_bus();
}

#endif /* CONFIG_PCI */
