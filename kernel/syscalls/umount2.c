/*
 * fiwix/kernel/syscalls/umount2.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/stat.h>
#include <fiwix/sleep.h>
#include <fiwix/devices.h>
#include <fiwix/buffer.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static struct resource umount_resource = { 0, 0 };

int sys_umount2(const char *target, int flags)
{
	struct inode *i_target;
	struct mount *mp = NULL;
	struct filesystems *fs;
	struct device *d;
	struct inode dummy_i;
	struct superblock *sb;
	char *tmp_target;
	__dev_t dev;
	int errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_umount2(%s, 0x%08x)\n", current->pid, target, flags);
#endif /*__DEBUG__ */

	if(!IS_SUPERUSER) {
		return -EPERM;
	}
	if((errno = malloc_name(target, &tmp_target)) < 0) {
		return errno;
	}
	if((errno = namei(tmp_target, &i_target, NULL, FOLLOW_LINKS))) {
		free_name(tmp_target);
		return errno;
	}
	if(!S_ISBLK(i_target->i_mode) && !S_ISDIR(i_target->i_mode)) {
		iput(i_target);
		free_name(tmp_target);
		return -EINVAL;
	}

	if(!(mp = get_mount_point(i_target))) {
		iput(i_target);
		free_name(tmp_target);
		return -EINVAL;
	}
	if(S_ISBLK(i_target->i_mode)) {
		dev = i_target->rdev;
	} else {
		dev = i_target->sb->dev;
	}

	if(!(sb = get_superblock(dev))) {
		printk("WARNING: %s(): unable to get superblock from device %d,%d\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
		iput(i_target);
		free_name(tmp_target);
		return -EINVAL;
	}

	/*
	 * We must free now the inode in order to avoid having its 'count' to 2
	 * when calling check_fs_busy(), specially if sys_umount() was called
	 * using the mount-point instead of the device.
	 */
	iput(i_target);
	free_name(tmp_target);

	if(check_fs_busy(dev, sb->root)) {
		return -EBUSY;
	}

	lock_resource(&umount_resource);

	fs = mp->fs;
	if(fs->fsop && fs->fsop->release_superblock) {
		fs->fsop->release_superblock(sb);
	}
	if(sb->fsop->flags & FSOP_REQUIRES_DEV) {
		if(!(d = get_device(BLK_DEV, dev))) {
			printk("WARNING: %s(): block device %d,%d not registered!\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
			unlock_resource(&umount_resource);
			return -EINVAL;
		}
		memset_b(&dummy_i, 0, sizeof(struct inode));
		dummy_i.dev = dummy_i.rdev = dev;
		if(d && d->fsop && d->fsop->close) {
			d->fsop->close(&dummy_i, NULL);
		}
	}

	sb->dir->mount_point = NULL;
	iput(sb->root);
	iput(sb->dir);

	sync_superblocks(dev);
	sync_inodes(dev);
	sync_buffers(dev);
	invalidate_buffers(dev);
	invalidate_inodes(dev);

	del_mount_point(mp);
	unlock_resource(&umount_resource);
	return 0;
}
