/*
 * fiwix/net/unix.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/errno.h>
#include <fiwix/socket.h>
#include <fiwix/net.h>
#include <fiwix/net/unix.h>
#include <fiwix/fcntl.h>
#include <fiwix/sched.h>
#include <fiwix/sleep.h>
#include <fiwix/mm.h>
#include <fiwix/string.h>
#include <fiwix/stdio.h>

#ifdef CONFIG_NET
struct unix_info *unix_socket_head;

static struct resource packet_resource = { 0, 0 };

static void add_unix_socket(struct unix_info *u)
{
	struct unix_info *h;

	if((h = unix_socket_head)) {
		while(h->next) {
			h = h->next;
		}
		h->next = u;
	} else {
		unix_socket_head = u;
	}
}

static void remove_unix_socket(struct unix_info *u)
{
	struct unix_info *h;

	if(unix_socket_head == u) {
		unix_socket_head = u->next;
		return;
	}

	h = unix_socket_head;
	while(h && h->next != u) {
		h = h->next;
	}
	if(h && h->next == u) {
		h->next = u->next;
	}
}

static struct unix_info *lookup_unix_socket(char *path, struct inode *i)
{
	struct unix_info *u;

	u = unix_socket_head;
	while(u) {
		if(u->sun) {
			if(!strcmp(u->sun->sun_path, path) && u->inode == i) {
				return u;
			}
		}
		u = u->next;
	}

	return NULL;
}

int unix_create(struct socket *s)
{
	struct unix_info *u;

	u = &s->u.unix;
	memset_b(u, 0, sizeof(struct unix_info));
	u->count = 1;
	u->socket = s;
	add_unix_socket(u);
	return 0;
}

void unix_free(struct socket *s)
{
	struct unix_info *u;

	u = &s->u.unix;
	if(!(--u->count)) {
		if(u->data) {
			kfree((unsigned int)u->data);
		}
		if(u->sun) {
			kfree((unsigned int)u->sun);
		}
		if(u->inode) {
			iput(u->inode);
		}
		u->peer = NULL;
		remove_unix_socket(u);
		return;
	}

	if(u->peer) {
		if(!--u->peer->count) {
			remove_unix_socket(u->peer);
		}
		if(u->peer->socket) {
			u->peer->socket->state = SS_DISCONNECTING;
		}
		wakeup(u->peer);
		wakeup(&do_select);
	}
	remove_unix_socket(u);
	return;
}

int unix_bind(struct socket *s, const struct sockaddr *addr, int addrlen)
{
	struct inode *i;
	struct sockaddr_un *su;
	int errno;

	su = (struct sockaddr_un *)addr;
	if(su->sun_family != AF_UNIX) {
                return -EINVAL;
	}
	if(addrlen < 0 || addrlen > sizeof(struct sockaddr_un)) {
                return -EINVAL;
	}

	if(s->u.unix.sun) {
		return -EINVAL;
	}
	if(!(s->u.unix.sun = (struct sockaddr_un *)kmalloc(sizeof(struct sockaddr_un)))) {
		return -ENOMEM;
	}
	memset_b(s->u.unix.sun, 0, sizeof(struct sockaddr_un));
	memcpy_b(s->u.unix.sun, su, addrlen);
	s->u.unix.sun_len = addrlen;

	errno = do_mknod((char *)su->sun_path, S_IFSOCK | (S_IRWXU | S_IRWXG | S_IRWXO), 0);
	if(errno < 0) {
		kfree((unsigned int)s->u.unix.sun);
		s->u.unix.sun = NULL;
		if(errno == -EEXIST) {
			errno = -EADDRINUSE;
		}
		return errno;
	}
	if((errno = namei(su->sun_path, &i, NULL, FOLLOW_LINKS))) {
		kfree((unsigned int)s->u.unix.sun);
		s->u.unix.sun = NULL;
		return errno;
	}
	s->u.unix.inode = i;
	return errno;
}

int unix_connect(struct socket *sc, const struct sockaddr *addr, int addrlen)
{
	struct inode *i;
	struct sockaddr_un *su;
	struct unix_info *up;
	char *tmp_name;
	int errno;

	su = (struct sockaddr_un *)addr;
	if(su->sun_family != AF_UNIX) {
                return -EINVAL;
	}
	if(addrlen < 0 || addrlen > sizeof(struct sockaddr_un)) {
                return -EINVAL;
	}

	if((errno = malloc_name(su->sun_path, &tmp_name)) < 0) {
		return errno;
	}
	if((errno = namei(tmp_name, &i, NULL, FOLLOW_LINKS))) {
		free_name(tmp_name);
		return errno;
	}
	if(!(up = lookup_unix_socket(tmp_name, i))) {
		iput(i);
		free_name(tmp_name);
		return -ECONNREFUSED;
	}
	iput(i);
	free_name(tmp_name);
	if((errno = insert_socket_to_queue(up->socket, sc))) {
		return errno;
	}
	wakeup(up->socket);
	sleep(sc, PROC_INTERRUPTIBLE);
	return 0;
}

int unix_accept(struct socket *sc, struct socket *nss)
{
	struct unix_info *uc, *us;

	uc = &sc->u.unix;
	us = &nss->u.unix;

	if(!(uc->data = (char *)kmalloc(PIPE_BUF))) {
		return -ENOMEM;
	}
	us->data = uc->data;
	us->sun = uc->sun;
	us->sun_len = uc->sun_len;
	us->peer = uc;
	us->count++;
	uc->peer = us;	/* server socket */
	uc->count++;
	sc->state = SS_CONNECTED;
	nss->state = SS_CONNECTED;
	wakeup(sc);
	wakeup(&do_select);
	return 0;
}

int unix_getname(struct socket *s, struct sockaddr *addr, int *addrlen, int call)
{
	struct unix_info *u;
	int len, errno;

	if((errno = check_user_area(VERIFY_WRITE, addrlen, sizeof(int)))) {
		return errno;
	}
	len = *addrlen;

	if(call == SYS_GETSOCKNAME) {
		u = &s->u.unix;
	} else {
		/* SYS_GETPEERNAME */
		u = s->u.unix.peer;
	}
	if(len > u->sun_len) {
		len = u->sun_len;
	}
	if(len) {
		if((errno = check_user_area(VERIFY_WRITE, addr, len))) {
			return errno;
		}
		memcpy_b(addr, u->sun, len);
	}
	return 0;
}

int unix_socketpair(struct socket *s1, struct socket *s2)
{
	struct unix_info *u1, *u2;

	u1 = &s1->u.unix;
	u2 = &s2->u.unix;

	if(!(u1->data = (char *)kmalloc(PIPE_BUF))) {
		return -ENOMEM;
	}
	u2->data = u1->data;
	u1->count++;
	u2->count++;
	u1->peer = u2;
	u2->peer = u1;
	s1->state = SS_CONNECTED;
	s2->state = SS_CONNECTED;
	return 0;
}

int unix_send(struct socket *s, struct fd *fd_table, const char *buffer, __size_t count, int flags)
{
	if(flags & ~MSG_DONTWAIT) {
		return -EINVAL;
	}
	return unix_write(s, fd_table, buffer, count);
}

int unix_recv(struct socket *s, struct fd *fd_table, char *buffer, __size_t count, int flags)
{
	if(flags & ~MSG_DONTWAIT) {
		return -EINVAL;
	}
	return unix_read(s, fd_table, buffer, count);
}

int unix_sendto(struct socket *s, struct fd *fd_table, const char *buffer, __size_t count, int flags, const struct sockaddr *addr, int addrlen)
{
	struct inode *i;
	struct unix_info *u;
	struct sockaddr_un *su;
	struct packet *p;
	char *tmp_name;
	int errno;

	su = (struct sockaddr_un *)addr;
	if(su->sun_family != AF_UNIX) {
                return -EINVAL;
	}
	if(addrlen < 0 || addrlen > sizeof(struct sockaddr_un)) {
                return -EINVAL;
	}

	if((errno = malloc_name(su->sun_path, &tmp_name)) < 0) {
		return errno;
	}
	if((errno = namei(tmp_name, &i, NULL, FOLLOW_LINKS))) {
		free_name(tmp_name);
		return errno;
	}
	if(!(u = lookup_unix_socket(tmp_name, i))) {
		iput(i);
		free_name(tmp_name);
		return -ECONNREFUSED;
	}
	iput(i);
	free_name(tmp_name);

	if(!(p = (struct packet *)kmalloc(sizeof(struct packet)))) {
		return -ENOMEM;
	}
	memset_b(p, 0, sizeof(struct packet));
	if(!(p->data = (char *)kmalloc(count + 1))) {
		kfree((unsigned int)p);
		return -ENOMEM;
	}
	memset_b(p->data, 0, count + 1);
	memcpy_b(p->data, buffer, count);
	p->len = count;
	p->socket = s;
	lock_resource(&packet_resource);
	append_packet_to_queue(p, &u->packet_queue);
	unlock_resource(&packet_resource);
	wakeup(u);
	return count;
}

int unix_recvfrom(struct socket *s, struct fd *fd_table, char *buffer, __size_t count, int flags, struct sockaddr *addr, int *addrlen)
{
	struct unix_info *u, *up;
	struct sockaddr_un *sun;
	struct packet *p;
	int size;

	u = &s->u.unix;

	lock_resource(&packet_resource);
	while(!(p = peek_packet(u->packet_queue))) {
		unlock_resource(&packet_resource);
		if(!(fd_table->flags & O_NONBLOCK)) {
			if(sleep(u, PROC_INTERRUPTIBLE)) {
				return -EINTR;
			}
			lock_resource(&packet_resource);
		} else {
			unlock_resource(&packet_resource);
			return -EAGAIN;
		}
	}

	size = MIN(p->len - p->offset, count);
	memcpy_b(buffer, p->data + p->offset, size);
	p->offset += size;
	if(!(flags & MSG_PEEK)) {
		p = remove_packet_from_queue(&u->packet_queue);
		kfree((unsigned int)p->data);
		kfree((unsigned int)p);
	}
	unlock_resource(&packet_resource);

	up = &p->socket->u.unix;
	sun = (struct sockaddr_un *)addr;
	sun->sun_family = AF_UNIX;
	memcpy_b(sun->sun_path, up->sun->sun_path, up->sun_len);
	*addrlen = up->sun_len;
	return size;
}

int unix_read(struct socket *s, struct fd *fd_table, char *buffer, __size_t count)
{
	struct unix_info *u;
	int bytes_read;
	int n, limit;

	u = &s->u.unix;
	bytes_read = 0;

	while(count) {
		if(u->writeoff) {
			if(u->readoff >= u->writeoff) {
				limit = PIPE_BUF - u->readoff;
			} else {
				limit = u->writeoff - u->readoff;
			}
		} else {
			limit = PIPE_BUF - u->readoff;
		}
		n = MIN(limit, count);
		if(u->size && n) {
			memcpy_b(buffer + bytes_read, u->data + u->readoff, n);
			bytes_read += n;
			u->readoff += n;
			u->size -= n;
			count -= n;
			if(u->writeoff == PIPE_BUF) {
				u->writeoff = 0;
			}
			wakeup(u->peer);
			wakeup(&do_select);
		} else {
			if(s->state != SS_CONNECTED) {
				if(s->state == SS_DISCONNECTING) {
					if(u->size) {
						if(u->readoff == PIPE_BUF) {
							u->readoff = 0;
						}
						continue;
					}
					return bytes_read;
				}
				return -EINVAL;
			}
			if(u->writeoff) {
				break;
			}
			if(fd_table->flags & O_NONBLOCK) {
				return -EAGAIN;
			}
			if(sleep(u, PROC_INTERRUPTIBLE)) {
				return -EINTR;
			}
		}
	}
	if(!u->size) {
		u->readoff = u->writeoff = 0;
	}
	return bytes_read;
}

int unix_write(struct socket *s, struct fd *fd_table, const char *buffer, __size_t count)
{
	struct unix_info *u, *up;
	int bytes_written;
	int n, limit;

	u = &s->u.unix;
	up = s->u.unix.peer;
	bytes_written = 0;

	while(bytes_written < count) {
		if(s->state != SS_CONNECTED) {
			if(s->state == SS_DISCONNECTING) {
				send_sig(current, SIGPIPE);
				return -EPIPE;
			}
			return -EINVAL;
		}
		if(up->readoff) {
			if(up->writeoff <= up->readoff) {
				limit = up->readoff;
			} else {
				limit = PIPE_BUF;
			}
		} else {
			limit = PIPE_BUF;
		}

		n = MIN((count - bytes_written), (limit - up->writeoff));

		if(n && n <= PIPE_BUF) {
			memcpy_b(up->data + up->writeoff, buffer + bytes_written, n);
			bytes_written += n;
			up->writeoff += n;
			up->size += n;
			if(up->readoff == PIPE_BUF) {
				up->readoff = 0;
			}
			wakeup(u->peer);
			wakeup(&do_select);
			continue;
		}
		wakeup(u->peer);
		wakeup(&do_select);
		if(!(fd_table->flags & O_NONBLOCK)) {
			if(sleep(u, PROC_INTERRUPTIBLE)) {
				return -EINTR;
			}
		} else {
			return -EAGAIN;
		}
	}
	return bytes_written;
}

int unix_select(struct socket *s, int flag)
{
	struct unix_info *u, *up;

	if(s->flags & SO_ACCEPTCONN) {
		if (flag == SEL_R && s->queue_len) {
			return 1;
		}
		return 0;
	}

	u = &s->u.unix;
	up = s->u.unix.peer;

	switch(flag) {
		case SEL_R:
			if(u->size) {
				return 1;
			}
			if(s->state != SS_CONNECTED) {
				printk("UNIX: select: socket not connected (read EOF)\n");
				return 1;
			}
			break;
		case SEL_W:
			if(s->state != SS_CONNECTED) {
				printk("UNIX: select: socket not connected (write EOF)\n");
				return 1;
			}
			if(up->size < PIPE_BUF) {
				return 1;
			}
			break;
	}
	return 0;
}

int unix_shutdown(struct socket *s, int how)
{
	return -EOPNOTSUPP;
}

int unix_setsockopt(struct socket *s, int level, int optname, const void *optval, socklen_t optlen)
{
	return -EOPNOTSUPP;
}

int unix_getsockopt(struct socket *s, int level, int optname, void *optval, socklen_t *optlen)
{
	return -EOPNOTSUPP;
}

int unix_init(void)
{
	unix_socket_head = NULL;
	return 0;
}
#endif /* CONFIG_NET */
