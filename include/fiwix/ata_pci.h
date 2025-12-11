/*
 * fiwix/include/fiwix/ata_pci.h
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_ATA_PCI_H
#define _FIWIX_ATA_PCI_H

#ifdef CONFIG_PCI
#include <fiwix/ata.h>

void ata_setup_dma(struct ide *, struct ata_drv *, char *, int, int);
void ata_start_dma(struct ide *, struct ata_drv *);
void ata_stop_dma(struct ide *, struct ata_drv *);
int ata_pci(struct ide *);
#endif /* CONFIG_PCI */

#endif /* _FIWIX_ATA_PCI_H */
