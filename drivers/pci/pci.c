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
static const char *get_strclass(unsigned short int class)
{
	switch(class) {
		case PCI_CLASS_STORAGE_IDE:		return "IDE interface";
		case PCI_CLASS_DISPLAY_VGA:		return "VGA Display controller";
		case PCI_CLASS_COMMUNICATION_SERIAL:	return "Serial controller";
	}
	return NULL;
}

static const char *get_strvendor_id(unsigned short int vendor_id)
{
#ifdef CONFIG_PCI_NAMES
	switch(vendor_id) {
		case PCI_VENDOR_ID_BOCHS:		return "QEMU";
		case PCI_VENDOR_ID_REDHAT:		return "Red Hat";
		case PCI_VENDOR_ID_INTEL:		return "Intel";
	}
#endif /* CONFIG_PCI_NAMES */
	return NULL;
}

static const char *get_strdevice_id(unsigned short int device_id)
{
#ifdef CONFIG_PCI_NAMES
	switch(device_id) {
		case PCI_DEVICE_ID_BGA:			return "Bochs Graphics Adapter";
		case PCI_DEVICE_ID_QEMU_16550A:		return "QEMU PCI 16550A";
		case PCI_DEVICE_ID_INTEL_82371SB_1:	return "82371SB IDE PIIX3 [Natoma]";
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
	struct pci_device *pdt;

	if(!(pdt = (struct pci_device *)kmalloc(sizeof(struct pci_device)))) {
		return;
	}

	*pdt = *pci_dev;
	if(!pci_device_table) {
		pci_device_table = pdt;
	} else {
		pdt->prev = pci_device_table->prev;
		pci_device_table->prev->next = pdt;
	}
	pci_device_table->prev = pdt;
}

static void scan_bus(void)
{
	int n, b, d, f;
	unsigned int vendor_id, device_id, class;
	unsigned char hdr_type, irq, prog_if;
	struct pci_device pci_dev, *pd;
	const char *name;
	unsigned int reg, addr, val;
	unsigned short int cmd;

	for(b = 0; b < PCI_MAX_BUS; b++) {
		for(d = 0; d < PCI_MAX_DEV; d++) {
			for(f = 0; f < PCI_MAX_FUNC; f++) {
				memset_b(&pci_dev, 0, sizeof(struct pci_device));
				pci_dev.bus = b;
				pci_dev.dev = d;
				pci_dev.func = f;

				vendor_id = pci_read_short(&pci_dev, PCI_VENDOR_ID);
				if(!vendor_id || vendor_id == 0xFFFF) {
					continue;
				}

				device_id = pci_read_short(&pci_dev, PCI_DEVICE_ID);
				prog_if = pci_read_char(&pci_dev, PCI_PROG_IF);
				class = pci_read_short(&pci_dev, PCI_CLASS_DEVICE);
				hdr_type = pci_read_char(&pci_dev, PCI_HEADER_TYPE);
				irq = pci_read_char(&pci_dev, PCI_INTERRUPT_LINE);

				printk("\t  b:%02x d:%02x f:%d", b, d, f);
				if(irq) {
					printk("   %3d", irq);
				} else {
					printk("     -");
				}
				printk("\t%04x:%04x %04x%02x", vendor_id, device_id, class, (int)prog_if);
				name = get_strclass(class);
				printk(" - %s\n", name ? name : "Unknown");
				pci_dev.vendor_id = vendor_id;
				pci_dev.device_id = device_id;
				pci_dev.class = class;
				pci_dev.hdr_type = hdr_type;
				pci_dev.irq = irq;

				pci_dev.command = pci_read_short(&pci_dev, PCI_COMMAND);
				pci_dev.status = pci_read_short(&pci_dev, PCI_STATUS);
				pci_dev.rev = pci_read_char(&pci_dev, PCI_REVISION_ID);
				pci_dev.prog_if = prog_if;
				pci_dev.cline_size = pci_read_char(&pci_dev, PCI_CACHE_LINE_SIZE);
				pci_dev.latency = pci_read_char(&pci_dev, PCI_LATENCY_TIMER);
				pci_dev.bist = pci_read_char(&pci_dev, PCI_BIST);
				pci_dev.pin = pci_read_char(&pci_dev, PCI_INTERRUPT_PIN);
				pci_dev.min_gnt = pci_read_char(&pci_dev, PCI_MIN_GRANT);
				pci_dev.max_lat = pci_read_char(&pci_dev, PCI_MAX_LATENCY);

				add_pci_device(b, d, f, &pci_dev);
				if(!f && !(hdr_type & 0x80)) {
					break;	/* no more functions in this device */
				}
			}
		}
	}

	/* get BARs */
	pd = pci_device_table;
	while(pd) {
		/* only normal PCI devices (bridges are not supported) */
		if(pd->hdr_type != PCI_HEADER_TYPE_NORMAL) {
			pd = pd->next;
			continue;
		}

		/* disable I/O space and address space */
		cmd = pci_read_short(pd, PCI_COMMAND);
		pci_write_short(pd, PCI_COMMAND, cmd & ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER));

		/* read up to 6 BARs */
		for(n = 0; n < 6; n++) {
			reg = PCI_BASE_ADDR_0 + (n * 4);

			/* save the original value */
			addr = pci_read_long(pd, reg);

			pci_write_long(pd, reg, ~0);
			val = pci_read_long(pd, reg);

			/* restore the original value */
			pci_write_long(pd, reg, addr);

			if(!val || val == 0xFFFFFFFF) {
				continue;
			}

			if(addr & PCI_BASE_ADDR_SPACE_IO) {
				addr &= PCI_BASE_ADDR_IO_MASK;
				pd->flags[n] |= PCI_F_ADDR_SPACE_IO;
				val = (~(val & ~0x1)) + 1;
			} else {
				/* memory mapped */
				if(addr & PCI_BASE_ADDR_MEM_PREF) {
					pd->flags[n] |= PCI_F_ADDR_SPACE_PREFET;
				}
				if((addr & PCI_BASE_ADDR_TYPE_MASK) == PCI_BASE_ADDR_TYPE_32) {
					pd->flags[n] |= PCI_F_ADDR_MEM_32;
				} 
				if((addr & PCI_BASE_ADDR_TYPE_MASK) == PCI_BASE_ADDR_TYPE_64) {
					pd->flags[n] |= PCI_F_ADDR_MEM_64;
				} 
				addr &= PCI_BASE_ADDR_MEM_MASK;
				pd->flags[n] |= PCI_F_ADDR_SPACE_MEM;
				val = (~(val & ~0xF)) + 1;
			}
			pd->bar[n] = addr;
			pd->size[n] = val;
		}

		/* restore I/O space and address space */
		pci_write_short(pd, PCI_COMMAND, cmd);

		pd = pd->next;
	}
}

unsigned char pci_read_char(struct pci_device *pci_dev, int offset)
{
	unsigned char retval;

	outport_l(PCI_ADDRESS, get_addr(pci_dev->bus, pci_dev->dev, pci_dev->func, offset));
	retval = inport_l(PCI_DATA) >> ((offset & 3) * 8) & 0xFF;
	return retval;
}

unsigned short int pci_read_short(struct pci_device *pci_dev, int offset)
{
	unsigned short int retval;

	outport_l(PCI_ADDRESS, get_addr(pci_dev->bus, pci_dev->dev, pci_dev->func, offset));
	retval = inport_l(PCI_DATA) >> ((offset & 2) * 8) & 0xFFFF;
	return retval;
}

unsigned int pci_read_long(struct pci_device *pci_dev, int offset)
{
	unsigned int retval;

	outport_l(PCI_ADDRESS, get_addr(pci_dev->bus, pci_dev->dev, pci_dev->func, offset));
	retval = inport_l(PCI_DATA) >> ((offset & 2) * 8);
	return retval;
}

void pci_write_char(struct pci_device *pci_dev, int offset, unsigned char buf)
{
	outport_l(PCI_ADDRESS, get_addr(pci_dev->bus, pci_dev->dev, pci_dev->func, offset));
	outport_b(PCI_DATA, buf);
}

void pci_write_short(struct pci_device *pci_dev, int offset, unsigned short int buf)
{
	outport_l(PCI_ADDRESS, get_addr(pci_dev->bus, pci_dev->dev, pci_dev->func, offset));
	outport_w(PCI_DATA, buf);
}

void pci_write_long(struct pci_device *pci_dev, int offset, unsigned int buf)
{
	outport_l(PCI_ADDRESS, get_addr(pci_dev->bus, pci_dev->dev, pci_dev->func, offset));
	outport_l(PCI_DATA, buf);
}

void pci_show_desc(struct pci_device *pci_dev)
{
	const char *name;

	printk("\t\t\t\tpci=b:%02x d:%02x f:%d, rev=0x%02x\n", pci_dev->bus, pci_dev->dev, pci_dev->func, pci_dev->rev);
	printk("\t\t\t\t");
	name = get_strdevice_id(pci_dev->device_id);
	printk("desc=%s ", name);
	name = get_strvendor_id(pci_dev->vendor_id);
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
	printk("     -\tbus range=0-%d, configuration type=1\n", PCI_MAX_BUS - 1);
	scan_bus();
}
#endif /* CONFIG_PCI */
