#ifdef CONFIG_NET

#ifndef _FIWIX_IN_H
#define _FIWIX_IN_H

#include <fiwix/socket.h>

#define IPPROTO_IP	0
#define IPPROTO_ICMP	1
#define IPPROTO_TCP	6
#define IPPROTO_UDP	17
#define IPPROTO_RAW	255

#if !defined(in_addr_t)
typedef unsigned int in_addr_t;
#endif

struct in_addr {
	in_addr_t s_addr;
};

#define __SIN_ZERO_SIZE__	(				\
	sizeof(struct sockaddr) -				\
	sizeof(short int) -					\
	sizeof(unsigned short int) -				\
	sizeof(struct in_addr))

struct sockaddr_in {
	short int sin_family;
	unsigned short int sin_port;
	struct in_addr sin_addr;
	unsigned char sin_zero[__SIN_ZERO_SIZE__];
};

#endif /* _FIWIX_IN_H */

#endif /* CONFIG_NET */
