/*
 * fiwix/kernel/syscalls/ustat.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/ustat.h>
#include <fiwix/statfs.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_ustat(__dev_t dev, struct ustat *ubuf)
{
	struct superblock *sb;
	struct statfs statfsbuf;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_ustat(%d, 0x%08x)\n", current->pid, dev, (int)ubuf);
#endif /*__DEBUG__ */
	if((errno = check_user_area(VERIFY_WRITE, ubuf, sizeof(struct ustat)))) {
		return errno;
	}
	if(!(sb = get_superblock(dev))) {
		return -EINVAL;
	}
	if(sb->fsop && sb->fsop->statfs) {
		sb->fsop->statfs(sb, &statfsbuf);
		memset_b(ubuf, 0, sizeof(struct ustat));
		ubuf->f_tfree = statfsbuf.f_bfree;
		ubuf->f_tinode = statfsbuf.f_ffree;
		return 0;
	}
	return -ENOSYS;
}
