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
struct packet *netdevice_queue;
struct bh netdevice_bh = { 0, &irq_netdevice_bh, NULL };
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

void irq_netdevice_bh(struct sigcontext *sc)
{
	struct netdevice *netdev;
	struct packet *pk;
	struct netif *netif;
	struct pbuf *p;

	if(can_lock_area(AREA_NETDEVICE)) {
		netdev = netdevice_table;	/* FIXME: don't parse lo */
		while(netdev) {
			while(netdev->queue) {
				pk = remove_packet_from_queue(&netdev->queue);
				netif = pk->lwip_netif;
				p = pk->lwip_pbuf;

				MIB2_STATS_NETIF_ADD(netif, ifinoctets, p->len);
				MIB2_STATS_NETIF_INC(netif, ifinucastpkts);	/* always unicast packets? */
#if ETH_PAD_SIZE
				pbuf_add_header(p, ETH_PAD_SIZE);	/* reclaim the padding word */
#endif /* ETH_PAD_SIZE */
				LINK_STATS_INC(link.recv);

				if(netif->input(p, netif) != ERR_OK) {
					LWIP_DEBUGF(NETIF_DEBUG, ("IP input error.\n"));
					pbuf_free(p);
				}
			}
			netdev = netdev->next;
		}
		unlock_area(AREA_NETDEVICE);
	} else {
		netdevice_bh.flags |= BH_ACTIVE;
	}
}

void netdevice_init(void)
{
	netdevice_table = NULL;
	if_count = ether_count = 0;

	loopback_init();
	netdevice_pci_init();
	if(ether_count) {
		add_bh(&netdevice_bh);
	}
}
#endif /* CONFIG_NET */
