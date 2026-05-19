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
/* lwIP prototypes */
int lwip_accept(int, struct sockaddr *, socklen_t *);
int lwip_bind(int, const struct sockaddr *, socklen_t);
int lwip_shutdown(int, int);
int lwip_getpeername (int, struct sockaddr *, socklen_t *);
int lwip_getsockname (int, struct sockaddr *, socklen_t *);
int lwip_getsockopt (int, int, int, void *, socklen_t *);
int lwip_setsockopt (int, int, int, const void *, socklen_t);
 int lwip_close(int);
int lwip_connect(int, const struct sockaddr *, socklen_t);
int lwip_listen(int, int backlog);
__ssize_t lwip_recv(int, void *, __size_t, int);
__ssize_t lwip_read(int, void *, __size_t);
__ssize_t lwip_readv(int, const struct iovec *, int);
__ssize_t lwip_recvfrom(int, void *, __size_t, int, struct sockaddr *, socklen_t *);
__ssize_t lwip_recvmsg(int, struct msghdr *, int);
__ssize_t lwip_send(int, const void *, __size_t, int);
__ssize_t lwip_sendmsg(int, const struct msghdr *, int);
__ssize_t lwip_sendto(int, const void *, __size_t, int, const struct sockaddr *, socklen_t);
int lwip_socket(int, int, int);
__ssize_t lwip_write(int, const void *, __size_t);
__ssize_t lwip_writev(int, const struct iovec *, int);
int lwip_ioctl(int, long, void *);


struct ipv4_info *ipv4_socket_head;

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

	if((fd = lwip_socket(domain, type, protocol)) < 0) {
		return fd;
	}
	s->fd_lwip = fd;
	ip4 = &s->u.ipv4_info;
	memset_b(ip4, 0, sizeof(struct ipv4_info));
	ip4->count = 1;
	ip4->socket = s;
	add_ipv4_socket(ip4);
	return 0;
}

void ipv4_free(struct socket *s)
{
	int errno;
	struct ipv4_info *ip4;

	if((errno = lwip_close(s->fd_lwip)) < 0) {
		return errno;
	}
	s->fd_lwip = 0;
	ip4 = &s->u.ipv4_info;
	remove_ipv4_socket(ip4);
	return errno;
}

int ipv4_bind(struct socket *s, const struct sockaddr *addr, int addrlen)
{
	return lwip_bind(s->fd_lwip, addr, addrlen);
}

int ipv4_listen(struct socket *s, int backlog)
{
	return lwip_listen(s->fd_lwip, backlog);
}

int ipv4_connect(struct socket *s, const struct sockaddr *addr, int addrlen)
{
	return lwip_connect(s->fd_lwip, addr, addrlen);
}

int ipv4_accept(struct socket *s, struct sockaddr *addr, unsigned int *addrlen)
{
	int fd, ufd;
	struct socket *sc;
	struct ipv4_info *ip4;

	if((fd = lwip_accept(s->fd_lwip, addr, addrlen)) < 0) {
		return fd;
	}

	sc = NULL;
	if((ufd = sock_alloc(&sc)) < 0) {
		return ufd;
	}
	sc->type = s->type;
	sc->ops = s->ops;
	sc->fd_lwip = fd;

	ip4 = &sc->u.ipv4_info;
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
	return lwip_sendto(s->fd_lwip, buffer, count, flags, addr, addrlen);
}

int ipv4_recvfrom(struct socket *s, struct fd *f, char *buffer, __size_t count, int flags, struct sockaddr *addr, socklen_t *addrlen)
{
	return lwip_recvfrom(s->fd_lwip, buffer, count, flags, addr, addrlen);
}

int ipv4_read(struct socket *s, struct fd *f, char *buffer, __size_t count)
{
	return lwip_read(s->fd_lwip, buffer, count);
}

int ipv4_write(struct socket *s, struct fd *f, const char *buffer, __size_t count)
{
	return lwip_write(s->fd_lwip, buffer, count);
}

int ipv4_ioctl(struct socket *s, struct fd *f, int cmd, unsigned int arg)
{
	int errno;

	if((errno = lwip_ioctl(s->fd_lwip, cmd, (void *)arg)) < 0) {
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
	return lwip_shutdown(s->fd_lwip, how);
}

int ipv4_setsockopt(struct socket *s, int level, int optname, const void *optval, socklen_t optlen)
{
	return lwip_setsockopt(s->fd_lwip, level, optname, optval, optlen);
}

int ipv4_getsockopt(struct socket *s, int level, int optname, void *optval, socklen_t *optlen)
{
	return lwip_getsockopt(s->fd_lwip, level, optname, optval, optlen);
}

int ipv4_init(void)
{
	ipv4_socket_head = NULL;
	return 0;
}
#endif /* CONFIG_NET */
