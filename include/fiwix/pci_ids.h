/*
 * fiwix/include/fiwix/pci_ids.h
 *
 * Copyright 2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_PCI_IDS_H
#define _FIWIX_PCI_IDS_H

/* device classes and subclasses */
#define PCI_CLASS_NOT_DEFINED		0x0000
#define PCI_CLASS_NOT_DEFINED_VGA	0x0001

#define PCI_CLASS_STORAGE_IDE		0x0101

#define PCI_CLASS_NETWORK_ETHERNET	0x0200

#define PCI_CLASS_DISPLAY_VGA		0x0300

#define PCI_CLASS_MULTIMEDIA_VIDEO	0x0400
#define PCI_CLASS_MULTIMEDIA_AUDIO	0x0401
#define PCI_CLASS_MULTIMEDIA_OTHER	0x0480

#define PCI_CLASS_BRIDGE_HOST		0x0600
#define PCI_CLASS_BRIDGE_ISA		0x0601
#define PCI_CLASS_BRIDGE_PCI		0x0604
#define PCI_CLASS_BRIDGE_OTHER		0x0680

#define PCI_CLASS_COMMUNICATION_SERIAL	0x0700

#define PCI_CLASS_SYSTEM_PIC		0x0800

#define PCI_CLASS_SERIAL_FIREWIRE	0x0c00
#define PCI_CLASS_SERIAL_USB		0x0c03


#define PCI_VENDOR_ID_REDHAT		0x1b36
#define PCI_DEVICE_QEMU_PCI_16550A	0x0002

#endif /* _FIWIX_PCI_IDS_H */
