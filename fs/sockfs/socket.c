/*
 * fiwix/fs/sockfs/socket.c
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_sock.h>
#include <fiwix/net.h>
#include <fiwix/string.h>

#ifdef CONFIG_NET
int sockfs_open(struct inode *i, struct fd *f)
{
	return -ENXIO;
}

int sockfs_close(struct inode *i, struct fd *f)
{
	struct socket *s;

	s = &i->u.sockfs.sock;
	sock_free(s);
	return 0;
}

int sockfs_read(struct inode *i, struct fd *f, char *buffer, __size_t count)
{
	struct socket *s;

	s = &i->u.sockfs.sock;
	return s->ops->read(s, f, buffer, count);
}

int sockfs_write(struct inode *i, struct fd *f, const char *buffer, __size_t count)
{
	struct socket *s;

	s = &i->u.sockfs.sock;
	return s->ops->write(s, f, buffer, count);
}

__loff_t sockfs_llseek(struct inode *i, __loff_t offset)
{
        return -ESPIPE;
}

int sockfs_select(struct inode *i, struct fd *f, int flag)
{
	struct socket *s;

	s = &i->u.sockfs.sock;
	return s->ops->select(s, flag);
}
#endif /* CONFIG_NET */
