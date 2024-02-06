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
#include <fiwix/stdio.h>

#ifdef CONFIG_NET
struct domain_table domains[] = {
        { AF_UNIX, "AF_UNIX", &unix_ops },
        { 0, 0, 0 }
};

struct proto_ops unix_ops = {
	unix_create,
	unix_free,
        unix_bind,
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
	unix_select,
	unix_shutdown,
	unix_setsockopt,
	unix_getsockopt,
        unix_init,
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
}
#endif /* CONFIG_NET */
