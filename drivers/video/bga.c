/*
 * fiwix/drivers/video/bga.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/kparms.h>
#include <fiwix/config.h>
#include <fiwix/bga.h>
#include <fiwix/pci.h>
#include <fiwix/console.h>
#include <fiwix/mm.h>
#include <fiwix/string.h>
#include <fiwix/stdio.h>

#ifdef CONFIG_PCI
#ifdef CONFIG_BGA
static struct pci_supported_devices supported[] = {
	{ PCI_VENDOR_ID_BOCHS, PCI_DEVICE_ID_BGA },
	{ 0, 0 }
};

static void bga_write_register(int index, int data)
{
	outport_w(VBE_DISPI_IOPORT_INDEX, index);
	outport_w(VBE_DISPI_IOPORT_DATA, data);
}

static unsigned short int bga_read_register(unsigned short int cmd)
{
	outport_w(VBE_DISPI_IOPORT_INDEX, cmd);
	return inport_w(VBE_DISPI_IOPORT_DATA);
}

void bga_init(void)
{
	struct pci_device *pci_dev;
	unsigned short int cmd;
	int xres, yres, bpp;

	if(!(pci_dev = pci_get_device(supported[0].vendor_id, supported[0].device_id))) {
		return;
	}
	if(!(*kparms.bgaresolution)) {
		return;
	}

	/* prepare to switch to the resolution requested */
	bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
	bga_write_register(VBE_DISPI_INDEX_XRES, video.fb_width);
	bga_write_register(VBE_DISPI_INDEX_YRES, video.fb_height);
	bga_write_register(VBE_DISPI_INDEX_BPP, video.fb_bpp);

	/* check if the values were accepted */
	xres = bga_read_register(VBE_DISPI_INDEX_XRES);
	yres = bga_read_register(VBE_DISPI_INDEX_YRES);
	bpp = bga_read_register(VBE_DISPI_INDEX_BPP);

	if(xres != video.fb_width || yres != video.fb_height || bpp != video.fb_bpp) {
		printk("WARNING: %s(): resolution %dx%dx%d is not valid.\n", __FUNCTION__, video.fb_width, video.fb_height, video.fb_bpp);
		return;
	}

	/* enable I/O space and memory space */
	cmd = (pci_dev->command | PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
	pci_write_short(pci_dev, PCI_COMMAND, cmd);

	video.pci_dev = pci_dev;
	video.address = (unsigned int *)pci_dev->bar[0];
	video.port = 0;

	strcpy((char *)video.signature, "BGA");
	video.fb_version = bga_read_register(VBE_DISPI_INDEX_ID);

	if(video.fb_version < VBE_DISPI_ID4) {
		video.memsize = 4 * 1024 * 1024;	/* 4MB */
	} else if(video.fb_version == VBE_DISPI_ID4) {
		video.memsize = 8 * 1024 * 1024;	/* 8MB */
	} else {
		video.memsize = bga_read_register(VBE_DISPI_INDEX_VIDEO_MEMORY_64K);
		video.memsize *= 64 * 1024;
	}

	video.fb_pixelwidth = video.fb_bpp / 8;
	video.fb_pitch = video.fb_width * video.fb_pixelwidth;
	video.fb_linesize = video.fb_pitch * video.fb_char_height;
	video.fb_size = video.fb_width * video.fb_height * video.fb_pixelwidth;
	video.fb_vsize = video.lines * video.fb_pitch * video.fb_char_height;

	map_kaddr(kpage_dir, (unsigned int)video.address, (unsigned int)video.address + video.memsize, 0, PAGE_PRESENT | PAGE_RW);

	bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
}
#endif /* CONFIG_BGA */
#endif /* CONFIG_PCI */
