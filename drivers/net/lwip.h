/*
 * fiwix/drivers/net/lwip.h
 *
 * Copyright 2026, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_NET

#include <lwip/netif.h>

void netdevice_lwip_callback(void *);

#endif /* CONFIG_NET */
