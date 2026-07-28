#ifdef CONFIG_NET

#ifndef _FIWIX_ROUTE_H
#define _FIWIX_ROUTE_H

#include <fiwix/socket.h>
#include <fiwix/netdevice.h>

struct rtentry
{
	unsigned long	rt_hash;
	struct sockaddr	rt_dst;
	struct sockaddr	rt_gateway;
	struct sockaddr	rt_genmask;
	short		rt_flags;
	short		rt_refcnt;
	unsigned long	rt_use;
	void		*rt_ifp;
	short		rt_metric;
	char		*rt_dev;
	unsigned long	rt_mss;
	unsigned long	rt_window;
	unsigned short	rt_irtt;
};

#define RTF_UP		0x0001		/* route usable */
#define RTF_GATEWAY	0x0002		/* destination is a gateway */

struct route {
	char name[IFNAMSIZ];
	unsigned int dst;
	unsigned int gw;
	unsigned int mask;
	short int flags;
	struct netdevice *netdev;
	struct route *prev;
	struct route *next;
};
extern struct route *route_table;

int route_add(struct rtentry *, struct netdevice *);
int route_del(struct rtentry *);
void route_init(void);

#endif /* _FIWIX_ROUTE_H */
#endif /* CONFIG_NET */
