/*
 * fiwix/drivers/block/dma.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/dma.h>
#include <fiwix/string.h>

/*
 *  DMA Channel  Page  Address  Count
 *  ---------------------------------
 *  0 (8 bit)     87h       0h     1h
 *  1 (8 bit)     83h       2h     3h
 *  2 (8 bit)     81h       4h     5h
 *  3 (8 bit)     82h       6h     7h
 *  4 (16 bit)    8Fh      C0h    C2h
 *  5 (16 bit)    8Bh      C4h    C6h
 *  6 (16 bit)    89h      C8h    CAh
 *  7 (16 bit)    8Ah      CCh    CEh
 */

#define LOW_BYTE(addr)	(addr & 0x00FF)
#define HIGH_BYTE(addr)	((addr & 0xFF00) >> 8)

unsigned char dma_mask[DMA_CHANNELS] =
	{ 0x0A, 0x0A, 0x0A, 0x0A, 0xD4, 0xD4, 0xD4, 0xD4 };
unsigned char dma_mode[DMA_CHANNELS] =
	{ 0x0B, 0x0B, 0x0B, 0x0B, 0xD6, 0xD6, 0xD6, 0xD6 };
unsigned char dma_clear[DMA_CHANNELS] =
	{ 0x0C, 0x0C, 0x0C, 0x0C, 0xD8, 0xD8, 0xD8, 0xD8 };
unsigned char dma_page[DMA_CHANNELS] =
	{ 0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A };
unsigned char dma_address[DMA_CHANNELS] =
	{ 0x00, 0x02, 0x04, 0x06, 0xC0, 0xC4, 0xC8, 0xCC };
unsigned char dma_count[DMA_CHANNELS] =
	{ 0x01, 0x03, 0x05, 0x07, 0xC2, 0xC6, 0xCA, 0xCE };

char *dma_resources[DMA_CHANNELS];


void start_dma(int channel, void *address, unsigned int count, int mode)
{
	/* setup (mask) the DMA channel */
	outport_b(dma_mask[channel], DMA_MASK_CHANNEL | channel);

	/* clear any data transfers that are currently executing */
	outport_b(dma_clear[channel], 0);

	/* set the specified mode */
	outport_b(dma_mode[channel], mode | channel);

	/* set the offset address */
	outport_b(dma_address[channel], LOW_BYTE((unsigned int)address));
	outport_b(dma_address[channel], HIGH_BYTE((unsigned int)address));

	/* set the physical page */
	outport_b(dma_page[channel], (unsigned int)address >> 16);

	/* the true (internal) length sent to the DMA is actually length + 1 */
	count--;

	/* set the length of the data */
	outport_b(dma_count[channel], LOW_BYTE(count));
	outport_b(dma_count[channel], HIGH_BYTE(count));

	/* clear the mask */
	outport_b(dma_mask[channel], DMA_UNMASK_CHANNEL | channel);
}

int dma_register(int channel, char *dev_name)
{
	if(dma_resources[channel]) {
		return 1;
	}
	dma_resources[channel] = dev_name;
	return 0;
}

int dma_unregister(int channel)
{
	if(!dma_resources[channel]) {
		return 1;
	}

	dma_resources[channel] = NULL;
	return 0;
}

void dma_init(void)
{
	memset_b(dma_resources, 0, sizeof(dma_resources));
}
