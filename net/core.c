/*
 * fiwix/net/core.c
 *
 * Copyright 2025, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/errno.h>
#include <fiwix/ioctl.h>
#include <fiwix/fs.h>
#include <fiwix/socket.h>
#include <fiwix/string.h>

#ifdef CONFIG_NET
int dev_ioctl(int cmd, void *arg)
{
	int n;

	switch(cmd) {
		default:
			return -EINVAL;
	}
}
#endif /* CONFIG_NET */
