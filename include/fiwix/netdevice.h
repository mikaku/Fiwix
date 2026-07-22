/*
 * fiwix/include/fiwix/netdevice.h
 *
 * Copyright 2026, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_NET
#ifdef CONFIG_PCI

#ifndef _FIWIX_NETDEVICE_H
#define _FIWIX_NETDEVICE_H

#include <fiwix/asm.h>
#include <fiwix/sigcontext.h>
#include <fiwix/pci.h>
#include <fiwix/if.h>
#include <fiwix/net/packet.h>

struct netdevice {
	char name[IFNAMSIZ];		/* lo, eth0, ... */
	char mac[8];			/* MAC address (6 + 2) */
	int num;			/* interface index */
	int flags;			/* IFF_UP, IFF_RUNNING, ... */
	unsigned short type;		/* ARPHRD_LOOPBACK, ... */
	unsigned short family;		/* AF_INET */
	void *lwip_netif;
	struct packet *queue;
	unsigned short int ioaddr;	/* I/O port address */
	struct pci_device *pci_dev;
	struct netdevice *next;

	/* NIC driver operations */
	int (*open)(struct netdevice *);
	int (*close)(struct netdevice *);
};
extern struct netdevice *netdevice_table;
extern struct bh netdevice_bh;

extern int if_count;
extern int ether_count;
int dev_ioctl(int, void *);

struct netdevice *netdevice_alloc(void);
void register_netdevice(struct netdevice *);
void irq_netdevice_bh(struct sigcontext *);
void netdevice_init(void);

/* NIC driver init function prototypes */
void rtl8139_init(struct pci_device *);

/* loopback driver prototypes */
int loopback_open(struct netdevice *);
void loopback_init(void);

#endif /* _FIWIX_NETDEVICE_H */

#endif /* CONFIG_PCI */
#endif /* CONFIG_NET */
