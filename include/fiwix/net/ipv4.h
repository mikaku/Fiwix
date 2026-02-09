/*
 * fiwix/include/fiwix/net/ipv4.h
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_NET

#ifndef _FIWIX_NET_IPV4_H
#define _FIWIX_NET_IPV4_H

#include <fiwix/types.h>

/* AF_INET */
struct ipv4_info {
	int count;
	struct socket *socket;
	struct ipv4_info *next;
};

extern struct ipv4_info *ipv4_socket_head;

extern struct proto_ops ipv4_ops;

int ipv4_create(struct socket *, int, int, int);
void ipv4_free(struct socket *);
int ipv4_bind(struct socket *, const struct sockaddr *, int);
int ipv4_listen(struct socket *, int);
int ipv4_connect(struct socket *, const struct sockaddr *, int);
int ipv4_accept(struct socket *, struct sockaddr *, int *);
int ipv4_getname(struct socket *, struct sockaddr *, int *, int);
int ipv4_socketpair(struct socket *, struct socket *);
int ipv4_send(struct socket *, struct fd *, const char *, __size_t, int);
int ipv4_recv(struct socket *, struct fd *, char *, __size_t, int);
int ipv4_sendto(struct socket *, struct fd *, const char *, __size_t, int, const struct sockaddr *, int);
int ipv4_recvfrom(struct socket *, struct fd *, char *, __size_t, int, struct sockaddr *, int *);
int ipv4_read(struct socket *, struct fd *, char *, __size_t);
int ipv4_write(struct socket *, struct fd *, const char *, __size_t);
int ipv4_ioctl(struct socket *, struct fd *, int, unsigned int);
int ipv4_select(struct socket *, int);
int ipv4_shutdown(struct socket *, int);
int ipv4_setsockopt(struct socket *, int, int, const void *, unsigned int);
int ipv4_getsockopt(struct socket *, int, int, void *, unsigned int *);
int ipv4_init(void);

#endif /* _FIWIX_NET_IPV4_H */

#endif /* CONFIG_NET */
