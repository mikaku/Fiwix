/*
 * fiwix/include/fiwix/netdevice.h
 *
 * Copyright 2026, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_NET

#ifndef _FIWIX_NETDEVICE_H
#define _FIWIX_NETDEVICE_H

#include <fiwix/if.h>

struct netdevice {
	char name[IFNAMSIZ];		/* lo, eth0, ... */
	int num;			/* interface index */
	int flags;			/* IFF_UP, IFF_RUNNING, ... */
	unsigned short type;		/* ARPHRD_LOOPBACK, ... */
	unsigned short family;		/* AF_INET */
	void *lwip_netif;
	struct netdevice *next;
};
extern struct netdevice *netdevice_table;

extern int if_count;
int dev_ioctl(int, void *);

struct netdevice *netdevice_alloc(void);
void register_netdevice(struct netdevice *);
void netdevice_init(void);

/* loopback driver prototypes */
int loopback_open(struct netdevice *);
void loopback_init(void);

#endif /* _FIWIX_NETDEVICE_H */

#endif /* CONFIG_NET */
