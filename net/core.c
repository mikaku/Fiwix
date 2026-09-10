/*
 * fiwix/net/core.c
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/errno.h>
#include <fiwix/ioctl.h>
#include <fiwix/fs.h>
#include <fiwix/if.h>
#include <fiwix/in.h>
#include <fiwix/route.h>
#include <fiwix/netdevice.h>
#include <fiwix/socket.h>
#include <fiwix/string.h>

#ifdef CONFIG_NET
#include <lwip/netif.h>

#define BY_INDEX	1
#define BY_NAME		2

static struct netdevice *netdev_find(int how, struct ifreq *ifr)
{
	struct netdevice *netdev;

	netdev = netdevice_table;
	while(netdev) {
		if(how == BY_NAME) {
			if(!strncmp(netdev->name, ifr->ifr_name, IFNAMSIZ)) {
				return netdev;
			}
		} else if(how == BY_INDEX) {
			if(netdev->num == ifr->ifr_ifindex) {
				return netdev;
			}
		}
		netdev = netdev->next;
	}
	return NULL;
}

static int dev_ifsioc(int cmd, void *arg)
{
	struct ifreq *ifr;
	struct netdevice *netdev;
	struct netif *netif;
	struct sockaddr_in *addr;
	int oldflags, retval;

	ifr = (struct ifreq *)arg;
	if((retval = check_user_area(VERIFY_WRITE, ifr, sizeof(struct ifreq)))) {
		return retval;
	}

	if(cmd != SIOCGIFNAME) {
		if(!(netdev = netdev_find(BY_NAME, ifr))) {
			return -ENODEV;
		}
		/*netif = netif_get_by_index(netdev->num + 1);	/* lwIP index starts at 1 */
		netif = (struct netif *)netdev->lwip_netif;
	}

	switch(cmd) {
		case SIOCGIFNAME:
			if(!(netdev = netdev_find(BY_INDEX, ifr))) {
				return -ENODEV;
			}
			strncpy(ifr->ifr_name, netdev->name, IFNAMSIZ);
			return 0;
		case SIOCGIFFLAGS:
			ifr->ifr_flags = netdev->flags;
			return 0;
		case SIOCSIFFLAGS:
			if(ifr->ifr_flags & ~(IFF_UP | IFF_BROADCAST | IFF_LOOPBACK | IFF_RUNNING)) {
				printk("WARNING: %s(): unsupported flags (%x).\n", __FUNCTION__, ifr->ifr_flags);
				return -EINVAL;
			}
			retval = 0;
			oldflags = netdev->flags;
			netdev->flags = ifr->ifr_flags & (IFF_UP | IFF_BROADCAST);
			if((oldflags ^ ifr->ifr_flags) & IFF_UP) {
				if(oldflags & IFF_UP) {
					netdev->flags &= ~(IFF_UP | IFF_RUNNING);
					retval = netdev->close(netdev);
				} else {
					netdev->flags |= (IFF_UP | IFF_RUNNING);
					if((retval = netdev->open(netdev)) < 0) {
						netdev->flags &= ~(IFF_UP | IFF_RUNNING);
					}
				}
			}
			return retval;
		case SIOCGIFADDR:
			ifr->ifr_addr.sa_family = netdev->family;
			addr = (struct sockaddr_in *)&ifr->ifr_addr;
			memcpy_b(&addr->sin_addr.s_addr, &netif->ip_addr.addr, 4);
			return 0;
		case SIOCSIFADDR:
			addr = (struct sockaddr_in *)&ifr->ifr_addr;
			netif_set_ipaddr(netif, (const ip4_addr_t *)&addr->sin_addr.s_addr);
			return 0;
		case SIOCGIFBRDADDR:
			ifr->ifr_addr.sa_family = netdev->family;
			addr = (struct sockaddr_in *)&ifr->ifr_addr;
			addr->sin_addr.s_addr = netif->ip_addr.addr | ~netif->netmask.addr;
			return 0;
		case SIOCSIFBRDADDR:
			return 0;
		case SIOCGIFNETMASK:
			ifr->ifr_addr.sa_family = netdev->family;
			addr = (struct sockaddr_in *)&ifr->ifr_addr;
			addr->sin_addr.s_addr = netif->netmask.addr;
			return 0;
		case SIOCSIFNETMASK:
			addr = (struct sockaddr_in *)&ifr->ifr_addr;
			if(!ip4_addr_netmask_valid(addr->sin_addr.s_addr)){
				return -EINVAL;
			}
			netif_set_netmask(netif, (const ip4_addr_t *)&addr->sin_addr.s_addr);
			return 0;
		case SIOCGIFMETRIC:
			ifr->ifr_metric = 1;
			return 0;
		case SIOCSIFMETRIC:
			return -EOPNOTSUPP;
		case SIOCGIFMTU:
			ifr->ifr_mtu = netif->mtu;
			return 0;
		case SIOCSIFMTU:
			netif->mtu = ifr->ifr_mtu;
			return 0;
		case SIOCGIFHWADDR:
			ifr->ifr_hwaddr.sa_family = netdev->type;
			memcpy_b(ifr->ifr_hwaddr.sa_data, netif->hwaddr, NETIF_MAX_HWADDR_LEN);
			return 0;
		case SIOCGIFINDEX:
			ifr->ifr_ifindex = netdev->num;
			return 0;
		case SIOCGIFMAP:
			ifr->ifr_map.mem_start = 0;
			ifr->ifr_map.mem_end = 0;
			ifr->ifr_map.base_addr = 0;
			ifr->ifr_map.irq = 0;
			ifr->ifr_map.dma = 0;
			ifr->ifr_map.port = 0;
			return 0;
		case SIOCSIFMAP:
			return -EOPNOTSUPP;
	}
	return -EINVAL;
}

static int dev_ifconf(void *arg)
{
	struct ifconf *ifc;
	struct ifreq *ifr;
	struct netdevice *netdev;
	struct netif *netif;
	struct sockaddr_in *addr;
	int retval, size;

	ifc = (struct ifconf *)arg;
	if((retval = check_user_area(VERIFY_WRITE, ifc, sizeof(struct ifconf)))) {
		return retval;
	}

	size = 0;
	netdev = netdevice_table;
	while(netdev) {
		if(size < ifc->ifc_len) {
			if(ifc->ifc_req) {
				if((retval = check_user_area(VERIFY_WRITE, ifc->ifc_req, sizeof(struct ifreq)))) {
					return retval;
				}
				netif = (struct netif *)netdev->lwip_netif;
				ifr = (struct ifreq *)(ifc->ifc_buf + size);
				memset(ifr, 0, sizeof(struct ifreq));
				strcpy(ifr->ifr_name, netdev->name);
				addr = (struct sockaddr_in *)&ifr->ifr_addr;
				addr->sin_family = netdev->family;
				memcpy_b(&addr->sin_addr.s_addr, &netif->ip_addr.addr, 4);
			}
			size += sizeof(struct ifreq);
		} else {
			break;
		}
		netdev = netdev->next;
	}
	ifc->ifc_len = size;
	return 0;
}

static int ip_rt_ioctl(unsigned int cmd, void *arg)
{
	struct rtentry rt;
	struct netdevice *netdev;
	struct netif *netif;
	struct ifreq ifr;
	struct in_addr gw;
	struct sockaddr_in *gwp;
	int retval;

	if((retval = check_user_area(VERIFY_READ, arg, sizeof(struct rtentry)))) {
		return retval;
	}
	memcpy_b(&rt, arg, sizeof(struct rtentry));

	strncpy(ifr.ifr_name, rt.rt_dev, IFNAMSIZ);
	if(!(netdev = netdev_find(BY_NAME, &ifr))) {
		return -ENODEV;
	}
	/*netif = netif_get_by_index(netdev->num + 1);	/* lwIP index starts at 1 */
	netif = (struct netif *)netdev->lwip_netif;

	retval = 0;
	switch(cmd) {
		case SIOCADDRT:
			gwp = (struct sockaddr_in *)&rt.rt_gateway;
			if(!(retval = route_add(&rt, netdev))) {
				netif_set_gw(netif, (const ip4_addr_t *)&gwp->sin_addr.s_addr);
			}
			break;
		case SIOCDELRT:
			gw.s_addr = 0;
			if(!(retval = route_del(&rt))) {
				netif_set_gw(netif, (const ip4_addr_t *)&gw.s_addr);
			}
			break;
	}
	return retval;
}

int dev_ioctl(int cmd, void *arg)
{
	switch(cmd) {
		case SIOCGIFCONF:
			return dev_ifconf(arg);
		case SIOCGIFNAME:
		case SIOCGIFFLAGS:
		case SIOCGIFADDR:
		case SIOCGIFBRDADDR:
		case SIOCGIFNETMASK:
		case SIOCGIFMETRIC:
		case SIOCGIFMTU:
		case SIOCGIFHWADDR:
		case SIOCGIFINDEX:
		case SIOCGIFMAP:
			return dev_ifsioc(cmd, arg);

		/* privileged operations (only for superuser) */
		case SIOCSIFFLAGS:
		case SIOCSIFADDR:
		case SIOCSIFBRDADDR:
		case SIOCSIFNETMASK:
		case SIOCSIFMETRIC:
		case SIOCSIFMTU:
		case SIOCSIFMAP:
			if(!IS_SUPERUSER) {
				return -EPERM;
			}
			return dev_ifsioc(cmd, arg);
		case SIOCADDRT:
		case SIOCDELRT:
			if(!IS_SUPERUSER) {
				return -EPERM;
			}
			return ip_rt_ioctl(cmd, arg);

		default:
			return -EINVAL;
	}
}
#endif /* CONFIG_NET */
