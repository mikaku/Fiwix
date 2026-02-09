/*
 * fiwix/net/domains.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/errno.h>
#include <fiwix/net.h>
#include <fiwix/socket.h>
#include <fiwix/string.h>

#ifdef CONFIG_NET
struct domain_table domains[] = {
        { AF_UNIX, "AF_UNIX", &unix_ops },
        { AF_INET, "AF_INET", &ipv4_ops },
        { 0, 0, 0 }
};

struct proto_ops unix_ops = {
	unix_create,
	unix_free,
	unix_bind,
	unix_listen,
	unix_connect,
	unix_accept,
	unix_getname,
	unix_socketpair,
	unix_send,
	unix_recv,
	unix_sendto,
	unix_recvfrom,
	unix_read,
	unix_write,
	unix_ioctl,
	unix_select,
	unix_shutdown,
	unix_setsockopt,
	unix_getsockopt,
	unix_init,
};

struct proto_ops ipv4_ops = {
	ipv4_create,
	ipv4_free,
	ipv4_bind,
	ipv4_listen,
	ipv4_connect,
	ipv4_accept,
	ipv4_getname,
	ipv4_socketpair,
	ipv4_send,
	ipv4_recv,
	ipv4_sendto,
	ipv4_recvfrom,
	ipv4_read,
	ipv4_write,
	ipv4_ioctl,
	ipv4_select,
	ipv4_shutdown,
	ipv4_setsockopt,
	ipv4_getsockopt,
	ipv4_init,
};

int assign_proto(struct socket *so, int domain)
{
	struct domain_table *d;

	d = &domains[0];
	while(d->ops) {
		if(d->family == domain) {
			so->ops = d->ops;
			return 0;
		}
		d++;
	}

	return -EINVAL;
}

void net_init(void)
{
	struct domain_table *d;
	struct proto_ops *ops;

	d = &domains[0];
	while(d->ops) {
		ops = d->ops;
		ops->init();
		d++;
	}

	/* call to the external TCP/IP API */
	ext_init();
}
#endif /* CONFIG_NET */
