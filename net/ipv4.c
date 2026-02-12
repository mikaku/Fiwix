/*
 * fiwix/net/ipv4.c
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>
#include <fiwix/socket.h>
#include <fiwix/net.h>
#include <fiwix/net/ipv4.h>
#include <fiwix/fcntl.h>
#include <fiwix/sched.h>
#include <fiwix/sleep.h>
#include <fiwix/mm.h>
#include <fiwix/string.h>
#include <fiwix/stdio.h>

#ifdef CONFIG_NET
struct ipv4_info *ipv4_socket_head;

static struct resource packet_resource = { 0, 0 };

static void add_ipv4_socket(struct ipv4_info *ip4)
{
	struct ipv4_info *h;

	if((h = ipv4_socket_head)) {
		while(h->next) {
			h = h->next;
		}
		h->next = ip4;
	} else {
		ipv4_socket_head = ip4;
	}
}

static void remove_ipv4_socket(struct ipv4_info *ip4)
{
	struct ipv4_info *h;

	if(ipv4_socket_head == ip4) {
		ipv4_socket_head = ip4->next;
		return;
	}

	h = ipv4_socket_head;
	while(h && h->next != ip4) {
		h = h->next;
	}
	if(h && h->next == ip4) {
		h->next = ip4->next;
	}
}

int ipv4_create(struct socket *s, int domain, int type, int protocol)
{
	int fd;
	struct ipv4_info *ip4;

	if((fd = ext_open(domain, type, protocol)) < 0) {
		return fd;
	}
	s->fd_ext = fd;

	ip4 = &s->u.ipv4_info;
	memset_b(ip4, 0, sizeof(struct ipv4_info));
	ip4->count = 1;
	ip4->socket = s;
	add_ipv4_socket(ip4);
	return fd;
}

void ipv4_free(struct socket *s)
{
	int errno;
	struct ipv4_info *ip4;

	if((errno = ext_close(s->fd_ext)) < 0) {
		return errno;
	}
	s->fd_ext = 0;
	ip4 = &s->u.ipv4_info;
	remove_ipv4_socket(ip4);
	return errno;
}

int ipv4_bind(struct socket *s, const struct sockaddr *addr, int addrlen)
{
	return ext_bind(s->fd_ext, addr, addrlen);
}

int ipv4_listen(struct socket *s, int backlog)
{
	return ext_listen(s->fd_ext, backlog);
}

int ipv4_connect(struct socket *s, const struct sockaddr *addr, int addrlen)
{
	return ext_connect(s->fd_ext, addr, addrlen);
}

int ipv4_accept(struct socket *s, struct sockaddr *addr, unsigned int *addrlen)
{
	int fd, ufd;
	struct socket *sc;
	struct ipv4_info *ip4;

	if((fd = ext_accept(s->fd_ext, addr, addrlen)) < 0) {
		return fd;
	}

	sc = NULL;
	if((ufd = sock_alloc(&sc)) < 0) {
		return ufd;
	}
	sc->type = s->type;
	sc->ops = s->ops;
	sc->fd_ext = fd;

	ip4 = &s->u.ipv4_info;
	memset_b(ip4, 0, sizeof(struct ipv4_info));
	ip4->count = 1;
	ip4->socket = sc;
	add_ipv4_socket(ip4);
	return ufd;
}

int ipv4_getname(struct socket *s, struct sockaddr *addr, unsigned int *addrlen, int call)
{
	return -EOPNOTSUPP;
}

int ipv4_socketpair(struct socket *s1, struct socket *s2)
{
	return -EOPNOTSUPP;
}

int ipv4_send(struct socket *s, struct fd *f, const char *buffer, __size_t count, int flags)
{
	if(flags & ~MSG_DONTWAIT) {
		return -EINVAL;
	}
	return ipv4_write(s, f, buffer, count);
}

int ipv4_recv(struct socket *s, struct fd *f, char *buffer, __size_t count, int flags)
{
	if(flags & ~MSG_DONTWAIT) {
		return -EINVAL;
	}
	return ipv4_read(s, f, buffer, count);
}

int ipv4_sendto(struct socket *s, struct fd *f, const char *buffer, __size_t count, int flags, const struct sockaddr *addr, int addrlen)
{
	return ext_sendto(s->fd_ext, buffer, count, addr, addrlen);
}

int ipv4_recvfrom(struct socket *s, struct fd *f, char *buffer, __size_t count, int flags, struct sockaddr *addr, int *addrlen)
{
	return ext_recvfrom(s->fd_ext, buffer, count, addr, addrlen);
}

int ipv4_read(struct socket *s, struct fd *f, char *buffer, __size_t count)
{
	return ext_read(s->fd_ext, buffer, count);
}

int ipv4_write(struct socket *s, struct fd *f, const char *buffer, __size_t count)
{
	return ext_write(s->fd_ext, buffer, count);
}

int ipv4_ioctl(struct socket *s, struct fd *f, int cmd, unsigned int arg)
{
	int errno;

	if((errno = ext_ioctl(s->fd_ext, cmd, (void *)arg)) < 0) {
		switch(cmd) {
			default:
				errno = dev_ioctl(cmd, (void *)arg);
				break;
		}
	}
	return errno;
}

int ipv4_select(struct socket *s, int flag)
{
	return -EOPNOTSUPP;
}

int ipv4_shutdown(struct socket *s, int how)
{
	return -EOPNOTSUPP;
}

int ipv4_setsockopt(struct socket *s, int level, int optname, const void *optval, socklen_t optlen)
{
	return -EOPNOTSUPP;
}

int ipv4_getsockopt(struct socket *s, int level, int optname, void *optval, socklen_t *optlen)
{
	return -EOPNOTSUPP;
}

int ipv4_init(void)
{
	ipv4_socket_head = NULL;
	return 0;
}
#endif /* CONFIG_NET */
