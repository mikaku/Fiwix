/*
 * fiwix/net/route.c
 *
 * Copyright 2026, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/errno.h>
#include <fiwix/if.h>
#include <fiwix/in.h>
#include <fiwix/route.h>
#include <fiwix/socket.h>
#include <fiwix/netdevice.h>
#include <fiwix/string.h>

#ifdef CONFIG_NET
#include <lwip/netif.h>

struct route *route_table;

int route_add(struct rtentry *rt, struct netdevice *netdev)
{
	struct route *r;
	struct sockaddr_in *ip;

	if(!(r = (struct route *)kmalloc(sizeof(struct route)))) {
		printk("WARNING: %s(): unable to allocate memory for a route.", __FUNCTION__);
		return -ENOMEM;
	}
	memset_b(r, 0, sizeof(struct route));
	strncpy(r->name, rt->rt_dev, IFNAMSIZ);
	ip = (struct sockaddr_in *)&rt->rt_dst;
	r->dst = (unsigned int)ip->sin_addr.s_addr;
	ip = (struct sockaddr_in *)&rt->rt_gateway;
	r->gw = (unsigned int)ip->sin_addr.s_addr;
	ip = (struct sockaddr_in *)&rt->rt_genmask;
	r->mask = (unsigned int)ip->sin_addr.s_addr;
	r->flags = rt->rt_flags;
	r->netdev = netdev;

	if(!route_table) {
		route_table = r;
	} else {
		r->prev = route_table->prev;
		route_table->prev->next = r;
	}
	route_table->prev = r;
	return 0;
}

int route_del(struct rtentry *rt)
{
	struct route *r;
	struct sockaddr_in *s_dst, *s_gw, *s_mask;
	unsigned int dst, gw, mask;

	s_dst = (struct sockaddr_in *)&rt->rt_dst;
	s_gw = (struct sockaddr_in *)&rt->rt_gateway;
	s_mask = (struct sockaddr_in *)&rt->rt_genmask;

	dst = (unsigned int)s_dst->sin_addr.s_addr;
	gw = (unsigned int)s_gw->sin_addr.s_addr;
	mask = (unsigned int)s_mask->sin_addr.s_addr;

	r = route_table;
	while(r) {
		if(!strncmp(r->name, rt->rt_dev, IFNAMSIZ) && r->dst == dst && r->gw == gw && r->mask == mask) {
			if(r->next) {
				r->next->prev = r->prev;
			}
			if(r->prev) {
				if(r != route_table) {
					r->prev->next = r->next;
				}
			}
			if(!r->next) {
				route_table->prev = r->prev;
			}
			if(r == route_table) {
				route_table = r->next;
			}
			return 0;
		}
		r = r->next;
	}
	return -ESRCH;
}

void route_init(void)
{
	route_table = NULL;
}
#endif /* CONFIG_NET */
