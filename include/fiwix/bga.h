/*
 * fiwix/include/fiwix/bga.h
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_BGA

#ifndef _FIWIX_BGA_H
#define _FIWIX_BGA_H

#define VBE_DISPI_IOPORT_INDEX		0x01CE
#define VBE_DISPI_IOPORT_DATA		0x01CF

#define VBE_DISPI_INDEX_ID		0x0
#define VBE_DISPI_INDEX_XRES		0x1
#define VBE_DISPI_INDEX_YRES		0x2
#define VBE_DISPI_INDEX_BPP		0x3
#define VBE_DISPI_INDEX_ENABLE		0x4
#define VBE_DISPI_INDEX_BANK		0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH	0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT	0x7
#define VBE_DISPI_INDEX_X_OFFSET	0x8
#define VBE_DISPI_INDEX_Y_OFFSET	0x9
#define VBE_DISPI_INDEX_VIDEO_MEMORY_64K	0xA

#define VBE_DISPI_ID4			0xB0C4

#define VBE_DISPI_DISABLED		0x00
#define VBE_DISPI_ENABLED		0x01
#define VBE_DISPI_LFB_ENABLED		0x40
#define VBE_DISPI_NOCLEARMEM		0x80

void bga_init(void);

#endif /* _FIWIX_BGA_H */

#endif /* CONFIG_BGA */
