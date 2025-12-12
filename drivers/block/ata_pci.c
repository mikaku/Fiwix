/*
 * fiwix/drivers/block/ata_pci.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/ata.h>
#include <fiwix/pci.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#ifdef CONFIG_PCI
static struct pci_supported_devices supported[] = {
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371SB_1 },	/* 82371SB PIIX3 [Natoma/Triton II] */
	{ 0, 0 }
};

void ata_setup_dma(struct ide *ide, struct ata_drv *drive, char *buffer, int datalen, int mode)
{
	int value;
	struct prd *prd_table = &drive->xfer.prd_table;

	prd_table->addr = (unsigned int)V2P(buffer);
	prd_table->size = datalen;
	prd_table->eot = PRDT_MARK_END;
	outport_l(ide->bm + BM_PRD_ADDRESS, V2P((unsigned int)prd_table));
	value = inport_b(ide->bm + BM_COMMAND);
	outport_b(ide->bm + BM_COMMAND, value | mode);

	/* clear Error and Interrupt bits */
	value = inport_b(ide->bm + BM_STATUS);
	outport_b(ide->bm + BM_STATUS, value | BM_STATUS_ERROR | BM_STATUS_INTR);
}

void ata_start_dma(struct ide *ide, struct ata_drv *drive)
{
	int value;

	value = inport_b(ide->bm + BM_COMMAND);
	outport_b(ide->bm + BM_COMMAND, value | BM_COMMAND_START);
}

void ata_stop_dma(struct ide *ide, struct ata_drv *drive)
{
	int status;

	inport_b(ide->bm + BM_STATUS);	/* extra read */
	status = inport_b(ide->bm + BM_STATUS);
	outport_b(ide->bm + BM_COMMAND, 0);	/* stop bus master */
	outport_b(ide->bm + BM_STATUS, status);
}

int ata_pci(struct ide *ide)
{
	struct pci_device *pci_dev;
	struct pci_supported_devices *supp;
	int bar, channel, found;
	int size;

	supp = (struct pci_supported_devices *)&supported;
	pci_dev = NULL;

	while(supp->vendor_id && supp->device_id) {
		if((pci_dev = pci_get_device(supp->vendor_id, supp->device_id))) {
			break;
		}
		supp++;
	}
	if(!supp->vendor_id && !supp->device_id) {
		return 0;
	}

	/* enable I/O space */
	pci_write_short(pci_dev, PCI_COMMAND, pci_dev->command | PCI_COMMAND_IO);

	for(found = 0, channel = 0; channel < NR_IDE_CTRLS; channel++, ide++) {
		bar = channel * 2;
		if(pci_dev->flags[bar] & PCI_F_ADDR_SPACE_MEM) {
			printk("MMIO-based BAR registers are not supported.\n");
			continue;
		}
		printk("ide%d	  ", channel);
		found++;
		ide_table_init(ide, channel);
		size = IDE_BASE_LEN;
		printk("0x%04x-0x%04x    %d\t", ide->base, ide->base + size, ide->irq);
		switch(pci_dev->prog_if & 0xF) {
			case 0:
				printk("ISA controller in compatibility mode-only\n");
				break;
			case 0xA:
				printk("ISA controller in compatibility mode, supports\n");
				printk("\t\t\t\tboth channels switched to PCI native mode\n");
				break;
			case 0x5:
			case 0xF:
				if((pci_dev->prog_if & 0xF) == 0x5) {
					printk("PCI controller in native mode-only\n");
				} else {
					printk("PCI controller in native mode, supports\n");
					printk("\t\t\t\tboth channels switched to ISA compatibility mode\n");
				}
				ide->base = pci_dev->bar[bar];
				ide->ctrl = pci_dev->bar[bar + 1];
				ide->irq = pci_dev->irq;
				size = pci_dev->size[bar];
				break;
		}
		if(pci_dev->bar[4] && (pci_dev->prog_if & 0x80)) {
			ide->bm = (pci_dev->bar[4] + (channel * 8));
			printk("\t\t\t\tbus master DMA at 0x%x\n", ide->bm);
			/* enable I/O space and bus master */
			pci_write_short(pci_dev, PCI_COMMAND, pci_dev->command | (PCI_COMMAND_IO | PCI_COMMAND_MASTER));
			ide->pci_dev = pci_dev;

			/* set PCI Latency Timer and transfers timing */
			switch(pci_dev->device_id) {
				case PCI_DEVICE_ID_INTEL_82371SB_1:
					pci_write_char(pci_dev, PCI_LATENCY_TIMER, 64);
					/* from the book 'FYSOS: Media Storage Devices', Appendix F */
					pci_write_short(pci_dev, 0x40, 0xA344);
					pci_write_short(pci_dev, 0x42, 0xA344);
					break;
			}
		}

		pci_show_desc(pci_dev);
		if(!ata_channel_init(ide)) {
			printk("\t\t\t\tno drives detected\n");
		}
	}

	return found;
}
#endif /* CONFIG_PCI */
