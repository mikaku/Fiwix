/*
 * fiwix/drivers/net/netdevice.c
 *
 * Copyright 2026, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/netdevice.h>
#include <fiwix/mm.h>
#include <fiwix/pci.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#ifdef CONFIG_NET
struct netdevice *netdevice_table;

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
        	while(netdev) {
                	netdev = netdev->next;
        	}
        	netdev = nd;
	}
}

void netdevice_init(void)
{
	netdevice_table = NULL;

	loopback_init();
}
#endif /* CONFIG_NET */
