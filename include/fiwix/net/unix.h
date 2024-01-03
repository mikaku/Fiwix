/*
 * fiwix/include/fiwix/net/unix.h
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_NET

#ifndef _FIWIX_NET_UNIX_H
#define _FIWIX_NET_UNIX_H

#include <fiwix/types.h>
#include <fiwix/net/packet.h>

/* AF_UNIX */
struct unix_info {
	int count;
	char *data;
	int readoff;
	int writeoff;
	int size;
	struct sockaddr_un *sun;
	short int sun_len;
	struct inode *inode;
	struct socket *socket;
	struct packet *packet_queue;
	struct unix_info *peer;
	struct unix_info *next;
};

extern struct unix_info *unix_socket_head;

extern struct proto_ops unix_ops;

int unix_create(struct socket *);
void unix_free(struct socket *);
int unix_bind(struct socket *, const struct sockaddr *, int);
int unix_connect(struct socket *, const struct sockaddr *, int);
int unix_accept(struct socket *, struct socket *);
int unix_getname(struct socket *, struct sockaddr *, int *, int);
int unix_socketpair(struct socket *, struct socket *);
int unix_send(struct socket *, struct fd *, const char *, __size_t, int);
int unix_recv(struct socket *, struct fd *, char *, __size_t, int);
int unix_sendto(struct socket *, struct fd *, const char *, __size_t, int, const struct sockaddr *, int);
int unix_recvfrom(struct socket *, struct fd *, char *, __size_t, int, struct sockaddr *, int *);
int unix_read(struct socket *, struct fd *, char *, __size_t);
int unix_write(struct socket *, struct fd *, const char *, __size_t);
int unix_select(struct socket *, int);
int unix_init(void);

#endif /* _FIWIX_NET_UNIX_H */

#endif /* CONFIG_NET */
