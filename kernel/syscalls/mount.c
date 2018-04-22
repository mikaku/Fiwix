/*
 * fiwix/kernel/syscalls/mount.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/buffer.h>
#include <fiwix/mman.h>
#include <fiwix/filesystems.h>
#include <fiwix/mm.h>
#include <fiwix/errno.h>
#include <fiwix/string.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

int sys_mount(const char *source, const char *target, const char *fstype, unsigned long int flags, const void *data)
{
	struct inode *i_source, *i_target;
	struct mount *mt;
	struct filesystems *fs;
	struct vma *vma;
	char *tmp_source, *tmp_target, *tmp_fstype;
	__dev_t dev;
	int len, errno;

#ifdef __DEBUG__
	printk("(pid %d) sys_mount(%s, %s, %s, 0x%08x, 0x%08x\n", current->pid, source, target, (int)fstype ? fstype : "<NULL>", flags, data);
#endif /* __DEBUG__ */

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
	if(!S_ISDIR(i_target->i_mode)) {
		iput(i_target);
		free_name(tmp_target);
		return -ENOTDIR;
	}
	if((flags & MS_MGC_VAL) == MS_MGC_VAL) {
		flags &= ~MS_MGC_MSK;
	}

	if(flags & MS_REMOUNT) {
		if(!(mt = get_mount_point(i_target))) {
			iput(i_target);
			free_name(tmp_target);
			return -EINVAL;
		}
		fs = mt->fs;
		if(fs->fsop && fs->fsop->remount_fs) {
			if((errno = fs->fsop->remount_fs(&mt->sb, flags))) {
				iput(i_target);
				free_name(tmp_target);
				return errno;
			}
		} else {
			iput(i_target);
			free_name(tmp_target);
			return -EINVAL;
		}

		/* switching from RW to RO */
		if(flags & MS_RDONLY && !(mt->sb.flags & MS_RDONLY)) {
			dev = mt->dev;
			/* 
			 * FIXME: if there are files opened in RW mode then
			 * we can't continue and must return -EBUSY.
			 */
			if(fs->fsop && fs->fsop->release_superblock) {
				fs->fsop->release_superblock(&mt->sb);
			}
			sync_superblocks(dev);
			sync_inodes(dev);
			sync_buffers(dev);
		}

		mt->sb.flags &= ~MS_RDONLY;
		mt->sb.flags |= (flags & MS_RDONLY);
		iput(i_target);
		free_name(tmp_target);
		return 0;
	}

	if(i_target->mount_point) {
		iput(i_target);
		free_name(tmp_target);
		return -EBUSY;
	}

	/* check the validity of fstype */
	if(!(vma = find_vma_region((unsigned int)fstype))) {
		iput(i_target);
		free_name(tmp_target);
		return -EFAULT;
	}
	if(!(vma->prot & PROT_READ)) {
		iput(i_target);
		free_name(tmp_target);
		return -EFAULT;
	}
	len = MIN(vma->end - (unsigned int)fstype, PAGE_SIZE - 1);
	if(!(tmp_fstype = (char *)kmalloc())) {
		iput(i_target);
		free_name(tmp_target);
		return -ENOMEM;
	}
	memcpy_b(tmp_fstype, fstype, len);

	if(!(fs = get_filesystem(fstype))) {
		iput(i_target);
		free_name(tmp_target);
		free_name(tmp_fstype);
		return -ENODEV;
	}
	dev = fs->fsop->fsdev;

	if((errno = malloc_name(source, &tmp_source)) < 0) {
		iput(i_target);
		free_name(tmp_target);
		free_name(tmp_fstype);
		return errno;
	}
	if(fs->fsop->flags == FSOP_REQUIRES_DEV) {
		if((errno = namei(tmp_source, &i_source, NULL, FOLLOW_LINKS))) {
			iput(i_target);
			free_name(tmp_target);
			free_name(tmp_fstype);
			free_name(tmp_source);
			return errno;
		}
		if(!S_ISBLK(i_source->i_mode)) {
			iput(i_target);
			iput(i_source);
			free_name(tmp_target);
			free_name(tmp_fstype);
			free_name(tmp_source);
			return -ENOTBLK;
		}
		if(i_source->fsop && i_source->fsop->open) {
			if((errno = i_source->fsop->open(i_source, NULL))) {
				iput(i_target);
				iput(i_source);
				free_name(tmp_target);
				free_name(tmp_fstype);
				free_name(tmp_source);
				return errno;
			}
		} else {
			iput(i_target);
			iput(i_source);
			free_name(tmp_target);
			free_name(tmp_fstype);
			free_name(tmp_source);
			return -EINVAL;
		}
		dev = i_source->rdev;
	}

	if(!(mt = get_free_mount_point(dev))) {
		if(fs->fsop->flags == FSOP_REQUIRES_DEV) {
			i_source->fsop->close(i_source, NULL);
			iput(i_source);
		}
		iput(i_target);
		free_name(tmp_target);
		free_name(tmp_fstype);
		free_name(tmp_source);
		return -EBUSY;
	}

	mt->sb.flags = flags;
	if(fs->fsop && fs->fsop->read_superblock) {
		if((errno = fs->fsop->read_superblock(dev, &mt->sb))) {
			i_source->fsop->close(i_source, NULL);
			if(fs->fsop->flags == FSOP_REQUIRES_DEV) {
				iput(i_source);
			}
			iput(i_target);
			release_mount_point(mt);
			free_name(tmp_target);
			free_name(tmp_fstype);
			free_name(tmp_source);
			return errno;
		}
	} else {
		if(fs->fsop->flags == FSOP_REQUIRES_DEV) {
			iput(i_source);
		}
		iput(i_target);
		release_mount_point(mt);
		free_name(tmp_target);
		free_name(tmp_fstype);
		free_name(tmp_source);
		return -EINVAL;
	}

	mt->dev = dev;
	strncpy(mt->devname, tmp_source, DEVNAME_MAX);
	strcpy(mt->dirname, tmp_target);
	mt->sb.dir = i_target;
	mt->fs = fs;
	i_target->mount_point = mt->sb.root;
	free_name(tmp_target);
	free_name(tmp_fstype);
	free_name(tmp_source);
	return 0;
}
