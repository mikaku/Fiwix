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

#ifdef CONFIG_PCI

/* supported device classes only */
static const char *pci_get_strclass(unsigned short int class)
{
	switch(class) {
		case PCI_CLASS_COMMUNICATION_SERIAL:	return "Serial controller";
	}
	return NULL;
}

const char *pci_get_strvendor_id(unsigned short int vendor_id)
{
#ifdef CONFIG_PCI_NAMES
	switch(vendor_id) {
		case PCI_VENDOR_ID_REDHAT:		return "Red Hat, Inc.";
	}
#endif
	return NULL;
}

const char *pci_get_strdevice_id(unsigned short int device_id)
{
#ifdef CONFIG_PCI_NAMES
	switch(device_id) {
		case PCI_DEVICE_QEMU_PCI_16550A:	return "QEMU PCI 16550A Adapter";
	}
#endif
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
	int orig, value;

	orig = inport_l(PCI_ADDRESS);

	outport_l(PCI_ADDRESS, 0x80000000);
	value = inport_l(PCI_ADDRESS);
	if(value == 0x80000000) {
		return 1;
	}

	outport_l(PCI_ADDRESS, orig);
	return 0;
}

static unsigned char pci_read_char(int bus, int dev, int func, int offset)
{
	unsigned char retval;

	outport_l(PCI_ADDRESS, get_addr(bus, dev, func, offset));
	retval = inport_l(PCI_DATA) >> ((offset & 3) * 8) & 0xFF;
	return retval;
}

static unsigned short int pci_read_short(int bus, int dev, int func, int offset)
{
	unsigned short int retval;

	outport_l(PCI_ADDRESS, get_addr(bus, dev, func, offset));
	retval = inport_l(PCI_DATA) >> ((offset & 2) * 8) & 0xFFFF;
	return retval;
}

static unsigned int pci_read_long(int bus, int dev, int func, int offset)
{
	unsigned int retval;

	outport_l(PCI_ADDRESS, get_addr(bus, dev, func, offset));
	retval = inport_l(PCI_DATA) >> ((offset & 2) * 8);
	return retval;
}

static void pci_write_long(int bus, int dev, int func, int offset, unsigned int buf)
{
	outport_l(PCI_ADDRESS, get_addr(bus, dev, func, offset));
	outport_l(PCI_DATA, buf);
}

static int pci_get_barsize(int bus, int dev, int func, int bar)
{
	int tmp, retval;

	/* backup original value */
	tmp = pci_read_long(bus, dev, func, bar);

	pci_write_long(bus, dev, func, bar, ~0);
	retval = pci_read_long(bus, dev, func, bar);
	retval = (~retval) + 1;

	/* restore original value */
	pci_write_long(bus, dev, func, bar, tmp);

	return retval;
}

static void pci_add_device(int bus, int dev, int func, struct pci_device *pci_dev)
{
	int n, bar;

	for(n = 0; n < NR_PCI_DEVICES; n++) {
		if(!pci_device_table[n].vendor_id) {

			/* fill in the rest of fields */
			pci_dev->command = pci_read_char(bus, dev, func, PCI_COMMAND);
			pci_dev->status = pci_read_char(bus, dev, func, PCI_STATUS);
			pci_dev->rev = pci_read_char(bus, dev, func, PCI_REVISION_ID);
			pci_dev->prog_if = pci_read_char(bus, dev, func, PCI_PROG_IF);
			for(bar = 0; bar < 6; bar++) {
				pci_dev->bar[bar] = pci_read_long(bus, dev, func, PCI_BASE_ADDRESS_0 + bar);
				if(pci_dev->bar[bar]) {
					pci_dev->size[bar] = pci_get_barsize(bus, dev, func, PCI_BASE_ADDRESS_0 + bar);
				}
			}
			pci_dev->pin = pci_read_char(bus, dev, func, PCI_INTERRUPT_PIN);
			pci_device_table[n] = *pci_dev;
			return;
		}
	}

	printk("WARNING: %s() no more PCI slots free.\n", __FUNCTION__);
}

static void scan_bus(void)
{
	int b, d, f;
	unsigned short int vendor_id, device_id, class;
	unsigned char header, irq;
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
				class = pci_read_short(b, d, f, PCI_CLASS_DEVICE);
				header = pci_read_char(b, d, f, PCI_HEADER_TYPE);
				irq = pci_read_char(b, d, f, PCI_INTERRUPT_LINE);

				printk("\t  b:%02x d:%02x f:%d", b, d, f);
				if(irq) {
					printk("   %3d", irq);
				} else {
					printk("     -");
				}
				printk("\t%04x:%04x %04x", vendor_id, device_id, class);

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
					pci_add_device(b, d, f, &pci_dev);
				}
				if(!f && !(header & 0x80)) {
					break;	/* no more functions in this device */
				}
			}
		}
	}
}

struct pci_device *pci_get_device(unsigned short int vendor_id, unsigned short int device_id)
{
	int n;

	for(n = 0; n < NR_PCI_DEVICES; n++) {
		if(pci_device_table[n].vendor_id == vendor_id &&
		   pci_device_table[n].device_id == device_id) {
			return &pci_device_table[n];
		}
	}

	return NULL;
}

void pci_init(void)
{
	if(!is_mechanism_1_supported()) {
		printk("WARNING: PCI mechanism 2 is not supported.\n");
		printk("WARNING: no PCI bus detected.\n");
		return;
	}

	printk("pci       0x%04x-0x%04x", PCI_ADDRESS, PCI_DATA + sizeof(unsigned int) - 1);
	printk("     -\tconfiguration type=1\n");

	memset_b(pci_device_table, 0, sizeof(pci_device_table));
	scan_bus();
}

#endif
