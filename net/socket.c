/*
 * fiwix/net/socket.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/asm.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/stat.h>
#include <fiwix/fcntl.h>
#include <fiwix/net.h>
#include <fiwix/socket.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/process.h>
#endif /*__DEBUG__ */

#ifdef CONFIG_NET
static int check_sd(int sd)
{
	struct inode *i;

	CHECK_UFD(sd);
	i = fd_table[current->fd[sd]].inode;
	if(!i || !S_ISSOCK(i->i_mode)) {
		return -ENOTSOCK;
	}

	return 0;
}

static struct socket *get_socket(int fd)
{
	struct inode *i;

	i = fd_table[current->fd[fd]].inode;
	return &i->u.sockfs.sock;
}

struct socket *get_socket_from_queue(struct socket *ss)
{
	unsigned int flags;
	struct socket *sc;

	sc = NULL;

	SAVE_FLAGS(flags); CLI();
	if((sc = ss->queue_head)) {
		ss->queue_head = sc->next_queue;
		ss->queue_len--;
	}
	RESTORE_FLAGS(flags);

	return sc;
}

/* append a socket to the list of pending connections */
int insert_socket_to_queue(struct socket *ss, struct socket *sc)
{
	unsigned int flags;
	struct socket *s;

	if(ss->queue_len + 1 > ss->queue_limit) {
		printk("WARNING: backlog exceeded!\n");
		return -ECONNREFUSED;
	}

	SAVE_FLAGS(flags); CLI();
	if((s = ss->queue_head)) {
		while(s->next_queue) {
			s = s->next_queue;
		}
		s->next_queue = sc;
	} else {
		ss->queue_head = sc;
	}
	RESTORE_FLAGS(flags);

	ss->queue_len++;
	return 0;
}

int sock_alloc(struct socket **s)
{
	int fd, ufd;
	struct filesystems *fs;
	struct inode *i;
	struct socket *ns;

	if(!(fs = get_filesystem("sockfs"))) {
		printk("WARNING: %s(): sockfs filesystem is not registered!\n", __FUNCTION__);
		return -EINVAL;
	}
	if(!(i = ialloc(&fs->mp->sb, S_IFSOCK))) {
		return -EINVAL;
	}
	if((fd = get_new_fd(i)) < 0) {
		iput(i);
		return -ENFILE;
	}
	if((ufd = get_new_user_fd(0)) < 0) {
		release_fd(fd);
		iput(i);
		return -EMFILE;
	}
	current->fd[ufd] = fd;
	i = fd_table[fd].inode;
	ns = &i->u.sockfs.sock;
	ns->state = SS_UNCONNECTED;
	fd_table[fd].flags = O_RDWR;
	ns->fd = &fd_table[fd];
	*s = ns;
	return ufd;
}

void sock_free(struct socket *s)
{
	int fd, ufd, n;
	struct inode *i;

	ufd = -1;

	/* pointer arithmetic */
	fd = ((unsigned int)s->fd - (unsigned int)&fd_table[0]) / sizeof(struct fd);

	for(n = 0; n < OPEN_MAX; n++) {
		if(current->fd[n] == fd) {
			ufd = n;
			break;
		}
	}

	if(ufd >= 0) {
		release_user_fd(ufd);
	}
	if(!(--fd_table[fd].count)) {
		i = s->fd->inode;
		iput(i);
		release_fd(fd);
	}
	if(s->ops) {
		s->ops->free(s);
	}
	wakeup(s);
}

int socket(int domain, int type, int protocol)
{
	int ufd;
	struct socket *s;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) socket(%d, %d, %d)\n", current->pid, domain, type, protocol);
#endif /*__DEBUG__ */

	if(type != SOCK_STREAM && type != SOCK_DGRAM) {
		return -EINVAL;
	}

	s = NULL;
	if((ufd = sock_alloc(&s)) < 0) {
		return ufd;
	}
	s->type = type;
	if(assign_proto(s, domain)) {
		sock_free(s);
		return -EINVAL;
	}
	if((errno = s->ops->create(s)) < 0) {
		sock_free(s);
		return errno;
	}
	return ufd;
}

int bind(int sd, struct sockaddr *addr, int addrlen)
{
	struct socket *s;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) bind(%d, 0x%08x, %d)\n", current->pid, sd, (int)addr, addrlen);
#endif /*__DEBUG__ */

	if((errno = check_sd(sd)) < 0) {
		return errno;
	}
	s = get_socket(sd);
	if((errno = check_user_area(VERIFY_READ, addr, addrlen))) {
		return errno;
	}
	return s->ops->bind(s, addr, addrlen);
}

int listen(int sd, int backlog)
{
	struct socket *ss;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) listen(%d, %d)\n", current->pid, sd, backlog);
#endif /*__DEBUG__ */

	if((errno = check_sd(sd)) < 0) {
		return errno;
	}
	ss = get_socket(sd);
	if(ss->type != SOCK_STREAM) {
		return -EOPNOTSUPP;
	}
	ss->flags |= SO_ACCEPTCONN;
	backlog = backlog < 0 ? 0 : backlog;
	ss->queue_limit = MIN(backlog, SOMAXCONN);
	return 0;
}

int connect(int sd, struct sockaddr *addr, int addrlen)
{
	struct socket *sc;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) connect(%d, 0x%08x, %d)\n", current->pid, sd, (int)addr, addrlen);
#endif /*__DEBUG__ */

	if((errno = check_sd(sd)) < 0) {
		return errno;
	}
	sc = get_socket(sd);
	if((errno = check_user_area(VERIFY_READ, addr, addrlen))) {
		return errno;
	}
	return sc->ops->connect(sc, addr, addrlen);
}

int accept(int sd, struct sockaddr *addr, int *addrlen)
{
	struct socket *ss;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) accept(%d, 0x%08x, 0x%08x)\n", current->pid, sd, (int)addr, addrlen);
#endif /*__DEBUG__ */

	if((errno = check_sd(sd)) < 0) {
		return errno;
	}
	ss = get_socket(sd);
	if(!(ss->flags & SO_ACCEPTCONN)) {
		return -EINVAL;
	}
	if(ss->type != SOCK_STREAM) {
		return -EOPNOTSUPP;
	}
	return ss->ops->accept(ss, addr, addrlen);
}

int getname(int sd, struct sockaddr *addr, int *addrlen, int call)
{
	struct socket *s;
	int errno;

#ifdef __DEBUG__
	if(call == SYS_GETSOCKNAME) {
		printk("(pid %d) getsockname(%d, 0x%08x, 0x%08x)\n", current->pid, sd, (int)addr, addrlen);
	} else {
		printk("(pid %d) getpeername(%d, 0x%08x, 0x%08x)\n", current->pid, sd, (int)addr, addrlen);
	}
#endif /*__DEBUG__ */

	if((errno = check_sd(sd)) < 0) {
		return errno;
	}
	s = get_socket(sd);
	return s->ops->getname(s, addr, addrlen, call);
}

int socketpair(int domain, int type, int protocol, int sockfd[2])
{
	int ufd1, ufd2;
	struct socket *s1, *s2;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) socketpair(%d, %d, %d, 0x%08x)\n", current->pid, domain, type, protocol, sockfd);
#endif /*__DEBUG__ */

	if((errno = check_user_area(VERIFY_WRITE, sockfd, sizeof(int) * 2))) {
		return errno;
	}

	/* create first socket */
	s1 = NULL;
	if((ufd1 = sock_alloc(&s1)) < 0) {
		return ufd1;
	}
	s1->type = type;
	if(assign_proto(s1, domain)) {
		sock_free(s1);
		return -EINVAL;
	}
	/* check if socketpair() is supported by the domain */
	if(!s1->ops->socketpair) {
		sock_free(s1);
		return -EINVAL;
	}
	if((errno = s1->ops->create(s1)) < 0) {
		sock_free(s1);
		return errno;
	}

	/* create second socket */
	s2 = NULL;
	if((ufd2 = sock_alloc(&s2)) < 0) {
		sock_free(s1);
		return ufd2;
	}
	s2->type = type;
	assign_proto(s2, domain);
	if((errno = s2->ops->create(s2)) < 0) {
		sock_free(s1);
		sock_free(s2);
		return errno;
	}

	if((errno = s1->ops->socketpair(s1, s2)) < 0) {
		sock_free(s1);
		sock_free(s2);
		return errno;
	}

	sockfd[0] = ufd1;
        sockfd[1] = ufd2;
	return 0;
}

int send(int sd, const void *buf, __size_t len, int flags)
{
	struct socket *s;
	struct fd fdt;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) send(%d, 0x%08x, %d, %d)\n", current->pid, sd, (int)buf, len, flags);
#endif /*__DEBUG__ */

	if((errno = check_sd(sd)) < 0) {
		return errno;
	}
	s = get_socket(sd);
	if((errno = check_user_area(VERIFY_READ, buf, len))) {
		return errno;
	}
	fdt.flags = s->fd->flags | ((flags & MSG_DONTWAIT) ? O_NONBLOCK : 0);
	return s->ops->send(s, &fdt, buf, len, flags);
}

int recv(int sd, void *buf, __size_t len, int flags)
{
	struct socket *s;
	struct fd fdt;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) recv(%d, 0x%08x, %d, %d)\n", current->pid, sd, (int)buf, len, flags);
#endif /*__DEBUG__ */

	if((errno = check_sd(sd)) < 0) {
		return errno;
	}
	s = get_socket(sd);
	if((errno = check_user_area(VERIFY_WRITE, buf, len))) {
		return errno;
	}
	fdt.flags = s->fd->flags | ((flags & MSG_DONTWAIT) ? O_NONBLOCK : 0);
	return s->ops->recv(s, &fdt, buf, len, flags);
}

int sendto(int sd, const void *buf, __size_t len, int flags, const struct sockaddr *addr, int addrlen)
{
	struct socket *s;
	struct fd fdt;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sendto(%d, 0x%08x, %d, %d, 0x%08x, %d)\n", current->pid, sd, (int)buf, len, flags, (int)addr, addrlen);
#endif /*__DEBUG__ */

	if((errno = check_sd(sd)) < 0) {
		return errno;
	}
	s = get_socket(sd);
	if((errno = check_user_area(VERIFY_READ, buf, len))) {
		return errno;
	}
	fdt.flags = s->fd->flags | ((flags & MSG_DONTWAIT) ? O_NONBLOCK : 0);
	return s->ops->sendto(s, &fdt, buf, len, flags, addr, addrlen);
}

int recvfrom(int sd, void *buf, __size_t len, int flags, struct sockaddr *addr, int *addrlen)
{
	struct socket *s;
	struct fd fdt;
	char ret_addr[108];
	int errno, ret_len, bytes_read;

#ifdef __DEBUG__
	printk("(pid %d) recvfrom(%d, 0x%08x, %d, %d, 0x%08x, 0x%08x)\n", current->pid, sd, (int)buf, len, flags, (int)addr, addrlen);
#endif /*__DEBUG__ */

	if((errno = check_sd(sd)) < 0) {
		return errno;
	}
	s = get_socket(sd);
	if((errno = check_user_area(VERIFY_WRITE, buf, len))) {
		return errno;
	}
	fdt.flags = s->fd->flags | ((flags & MSG_DONTWAIT) ? O_NONBLOCK : 0);
	memset_b(ret_addr, 0, 108);
	if((errno = s->ops->recvfrom(s, &fdt, buf, len, flags, (struct sockaddr *)ret_addr, &ret_len)) < 0) {
		return errno;
	}
	bytes_read = errno;
	if(ret_len && addr) {
		if((errno = check_user_area(VERIFY_WRITE, addr, ret_len))) {
			return errno;
		}
		memcpy_b(addr, ret_addr, ret_len);
	}
	return bytes_read;
}

int shutdown(int sd, int how)
{
	struct socket *s;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) shutdown(%d, %d)\n", current->pid, sd, how);
#endif /*__DEBUG__ */

	if((errno = check_sd(sd)) < 0) {
		return errno;
	}
	s = get_socket(sd);
	return s->ops->shutdown(s, how);
}

int setsockopt(int sd, int level, int optname, const void *optval, socklen_t optlen)
{
	struct socket *s;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) setsockopt(%d, %d, %d, %x, %d)\n", current->pid, sd, level, optname, (int)optval, optlen);
#endif /*__DEBUG__ */

	if((errno = check_sd(sd)) < 0) {
		return errno;
	}
	s = get_socket(sd);
	return s->ops->setsockopt(s, level, optname, optval, optlen);
}

int getsockopt(int sd, int level, int optname, void *optval, socklen_t *optlen)
{
	struct socket *s;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) getsockopt(%d, %d, %d, %x, %x)\n", current->pid, sd, level, optname, (int)optval, (int)optlen);
#endif /*__DEBUG__ */

	if((errno = check_sd(sd)) < 0) {
		return errno;
	}
	s = get_socket(sd);
	return s->ops->getsockopt(s, level, optname, optval, optlen);
}
#endif /* CONFIG_NET */
