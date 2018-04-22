/*
 * fiwix/include/fiwix/dma.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_DMA_H
#define _FIWIX_DMA_H

#define DMA_CHANNELS		8	/* max. number of DMA channels */

#define DMA_MASK_CHANNEL	0x04
#define DMA_UNMASK_CHANNEL	0x00

#define DMA_MODE_VERIFY		0x00
#define DMA_MODE_WRITE		0x04	/* read device -> write memory */
#define DMA_MODE_READ		0x08	/* read memory -> write device */
#define DMA_MODE_AUTOINIT	0x10
#define DMA_MODE_ADDRES_DEC	0x20
#define DMA_MODE_DEMAND		0x00
#define DMA_MODE_SINGLE		0x40
#define DMA_MODE_BLOCK		0x80
#define DMA_MODE_CASCADE	0xC0

char *dma_resources[DMA_CHANNELS];

void start_dma(int, void *, unsigned int, int);
int dma_register(int, char *);
int dma_unregister(int);
void dma_init(void);

#endif /* _FIWIX_DMA_H */
