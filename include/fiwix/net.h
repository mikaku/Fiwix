/*
 * fiwix/include/fiwix/net.h
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_NET

#ifndef _FIWIX_NET_H
#define _FIWIX_NET_H

#include <fiwix/types.h>
#include <fiwix/socket.h>
#include <fiwix/fd.h>
#include <fiwix/net/unix.h>

#define SYS_SOCKET	1
#define SYS_BIND	2
#define SYS_CONNECT	3
#define SYS_LISTEN	4
#define SYS_ACCEPT	5
#define SYS_GETSOCKNAME	6
#define SYS_GETPEERNAME	7
#define SYS_SOCKETPAIR	8
#define SYS_SEND	9
#define SYS_RECV	10
#define SYS_SENDTO	11
#define SYS_RECVFROM	12
#define SYS_SHUTDOWN	13
#define SYS_SETSOCKOPT	14
#define SYS_GETSOCKOPT	15

typedef unsigned int	socklen_t;

struct socket {
	short int state;
	int flags;
	struct fd *fd;
	short int type;
	struct proto_ops *ops;
	int queue_len;			/* number of connections in queue */
	int queue_limit;		/* max. number of pending connections */
	struct socket *queue_head;	/* first connection in queue */
	struct socket *next_queue;	/* next connection in queue */
	union {
		struct unix_info unix_info;
	} u;
};

struct domain_table {
	int family;
	char *name;
	struct proto_ops *ops;
};

struct proto_ops {
	int (*create)(struct socket *);
	void (*free)(struct socket *);
	int (*bind)(struct socket *, const struct sockaddr *, int);
	int (*connect)(struct socket *, const struct sockaddr *, int);
	int (*accept)(struct socket *, struct sockaddr *, int *);
	int (*getname)(struct socket *, struct sockaddr *, int *, int);
	int (*socketpair)(struct socket *, struct socket *);
	int (*send)(struct socket *, struct fd *, const char *, __size_t, int);
	int (*recv)(struct socket *, struct fd *, char *, __size_t, int);
	int (*sendto)(struct socket *, struct fd *, const char *, __size_t, int, const struct sockaddr *, int);
	int (*recvfrom)(struct socket *, struct fd *, char *, __size_t, int, struct sockaddr *, int *);
	int (*read)(struct socket *, struct fd *, char *, __size_t);
	int (*write)(struct socket *, struct fd *, const char *, __size_t);
	int (*ioctl)(struct socket *, struct fd *, int, unsigned int);
	int (*select)(struct socket *, int);
	int (*shutdown)(struct socket *, int);
	int (*setsockopt)(struct socket *, int, int, const void *, unsigned int);
	int (*getsockopt)(struct socket *, int, int, void *, unsigned int *);
	int (*init)(void);
};

int assign_proto(struct socket *, int);

struct socket *get_socket_from_queue(struct socket *);
int insert_socket_to_queue(struct socket *, struct socket *);
int sock_alloc(struct socket **);
void sock_free(struct socket *);
int socket(int, int, int);
int bind(int, struct sockaddr *, int);
int listen(int, int);
int connect(int, struct sockaddr *, int);
int accept(int, struct sockaddr *, int *);
int getname(int, struct sockaddr *, int *, int);
int socketpair(int, int, int, int [2]);
int send(int, const void *, __size_t, int);
int recv(int, void *, __size_t, int);
int sendto(int, const void *, __size_t, int, const struct sockaddr *, int);
int recvfrom(int, void *, __size_t, int, struct sockaddr *, int *);
int shutdown(int, int);
int setsockopt(int, int, int, const void *, socklen_t);
int getsockopt(int, int, int, void *, socklen_t *);

void net_init(void);

#endif /* _FIWIX_NET_H */

#endif /* CONFIG_NET */
