/*
 * fiwix/drivers/net/netdevice.c
 *
 * Copyright 2026, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/irq.h>
#include <fiwix/sleep.h>
#include <fiwix/netdevice.h>
#include <fiwix/mm.h>
#include <fiwix/pci.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#ifdef CONFIG_NET
#include <lwip/netif.h>
#include <lwip/snmp.h>

struct netdevice *netdevice_table;
int if_count, ether_count;

static void netdevice_pci_init(void)
{
#ifdef CONFIG_PCI
	struct pci_device *pci_dev;

	pci_dev = pci_device_table;
	while(pci_dev) {
		if(pci_dev->class == PCI_CLASS_NETWORK_ETHERNET) {
			switch(pci_dev->vendor_id) {
				case PCI_VENDOR_ID_REALTEK:
					switch(pci_dev->device_id) {
						case PCI_DEVICE_ID_REALTEK_8139:
							rtl8139_init(pci_dev);
							break;
					}
					break;
			}
		}
		pci_dev = pci_dev->next;
	}
#endif /* CONFIG_PCI */
}

struct netdevice *netdevice_alloc(void)
{
	struct netdevice *nd;

	if(!(nd = (struct netdevice *)kmalloc(sizeof(struct netdevice)))) {
		printk("WARNING: %s(): unable to allocate memory for network device.", __FUNCTION__);
		return NULL;
	}
	memset_b(nd, 0, sizeof(struct netdevice));
	return nd;
}

void register_netdevice(struct netdevice *nd)
{
	struct netdevice *netdev;

	if(!netdevice_table) {
		netdevice_table = nd;
	} else {
		netdev = netdevice_table;
		while(netdev->next) {
                	netdev = netdev->next;
        	}
		netdev->next = nd;
	}
}

void netdevice_init(void)
{
	netdevice_table = NULL;
	if_count = ether_count = 0;

	loopback_init();
	netdevice_pci_init();
}
#endif /* CONFIG_NET */
