/*
 * fiwix/drivers/net/loopback.c
 *
 * Copyright 2026, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/netdevice.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#ifdef CONFIG_NET
#include <lwip/netif.h>

void loopback_init(void)
{
	struct netdevice *nd;
	struct netif *netif;

	if(!(nd = netdevice_alloc())) {
		return;
	}
	netif = netif_get_by_index(1);
	netif->mtu = 4096;
	sprintk(nd->name, "%s", netif->name);
	nd->num = if_count;
	nd->flags = IFF_UP | IFF_LOOPBACK | IFF_RUNNING;
	nd->type = ARPHRD_LOOPBACK;
	nd->family = AF_INET;
	nd->lwip_netif = (void *)netif;
	register_netdevice(nd);
	if_count++;
}
#endif /* CONFIG_NET */
