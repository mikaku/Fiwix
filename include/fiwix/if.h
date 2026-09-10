#ifdef CONFIG_NET

#ifndef _FIWIX_IF_H
#define _FIWIX_IF_H

#include <fiwix/socket.h>

/* standard interface flags */
#define IFF_UP		0x1		/* interface is up */
#define IFF_BROADCAST	0x2		/* broadcast address valid */
#define IFF_LOOPBACK	0x8		/* is a loopback net */
#define IFF_RUNNING	0x40		/* resources allocated */

#define ARPHRD_ETHER	1
#define ARPHRD_LOOPBACK	772

struct ifaddr
{
	struct sockaddr ifa_addr;
	union {
		struct sockaddr ifu_broadaddr;
		struct sockaddr ifu_dstaddr;
	} ifa_ifu;
	struct iface *ifa_ifp;
	struct ifaddr *ifa_next;
};

#define ifa_broadaddr	ifa_ifu.ifu_broadaddr	/* broadcast address */
#define ifa_dstaddr	ifa_ifu.ifu_dstaddr	/* other end of link */

struct ifmap
{
	unsigned long int mem_start;
	unsigned long int mem_end;
	unsigned short int base_addr;
	unsigned char irq;
	unsigned char dma;
	unsigned char port;
};

#define IFHWADDRLEN	6
#define IFNAMSIZ	16

struct ifreq
{
	union {
		char ifrn_name[IFNAMSIZ];
	} ifr_ifrn;
	union {
		struct sockaddr ifru_addr;
		struct sockaddr ifru_dstaddr;
		struct sockaddr ifru_broadaddr;
		struct sockaddr ifru_netmask;
		struct sockaddr ifru_hwaddr;
		short ifru_flags;
		int ifru_ivalue;
		int ifru_mtu;
		struct ifmap ifru_map;
		char ifru_slave[IFNAMSIZ];
		char ifru_newname[IFNAMSIZ];
		void *ifru_data;
	} ifr_ifru;
};

#define ifr_name	ifr_ifrn.ifrn_name	/* interface name */
#define ifr_hwaddr	ifr_ifru.ifru_hwaddr	/* MAC address */
#define ifr_addr	ifr_ifru.ifru_addr	/* IP address */
#define ifr_dstaddr	ifr_ifru.ifru_dstaddr	/* other end of p-p lnk */
#define ifr_broadaddr	ifr_ifru.ifru_broadaddr	/* broadcast address */
#define ifr_netmask	ifr_ifru.ifru_netmask	/* interface net mask */
#define ifr_flags	ifr_ifru.ifru_flags	/* flags */
#define ifr_metric	ifr_ifru.ifru_ivalue	/* metric */
#define ifr_mtu		ifr_ifru.ifru_mtu	/* mtu */
#define ifr_map		ifr_ifru.ifru_map	/* device map */
#define ifr_slave	ifr_ifru.ifru_slave	/* slave device */
#define ifr_data	ifr_ifru.ifru_data	/* for use by interface */
#define ifr_ifindex	ifr_ifru.ifru_ivalue	/* interface index */
#define ifr_newname	ifr_ifru.ifru_newname

struct ifconf
{
	int ifc_len;
	union {
		void *ifcu_buf;
		struct ifreq *ifcu_req;
	} ifc_ifcu;
};

#define ifc_buf		ifc_ifcu.ifcu_buf
#define ifc_req		ifc_ifcu.ifcu_req

#endif /* _FIWIX_IF_H */

#endif /* CONFIG_NET */
