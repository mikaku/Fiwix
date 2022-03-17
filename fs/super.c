/*
 * fiwix/fs/super.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/filesystems.h>
#include <fiwix/sleep.h>
#include <fiwix/sched.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct mount *mount_table;
static struct resource sync_resource = { 0, 0 };

void superblock_lock(struct superblock *sb)
{
	unsigned long int flags;

	for(;;) {
		SAVE_FLAGS(flags); CLI();
		if(sb->locked) {
			sb->wanted = 1;
			RESTORE_FLAGS(flags);
			sleep(&superblock_lock, PROC_UNINTERRUPTIBLE);
		} else {
			break;
		}
	}
	sb->locked = 1;
	RESTORE_FLAGS(flags);
}
 
void superblock_unlock(struct superblock *sb)
{
	unsigned long int flags;

	SAVE_FLAGS(flags); CLI();
	sb->locked = 0;
	if(sb->wanted) {
		sb->wanted = 0;
		wakeup(&superblock_lock);
	}
	RESTORE_FLAGS(flags);
}

struct mount * get_free_mount_point(__dev_t dev)
{
	unsigned long int flags;
	int n;

	if(!dev) {
		printk("%s(): invalid device %d,%d.\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
		return NULL;
	}

	for(n = 0; n < NR_MOUNT_POINTS; n++) {
		if(mount_table[n].dev == dev) {
			printk("%s(): device %d,%d already mounted.\n", __FUNCTION__, MAJOR(dev), MINOR(dev));
			return NULL;
		}
	}

	SAVE_FLAGS(flags); CLI();
	for(n = 0; n < NR_MOUNT_POINTS; n++) {
		if(!mount_table[n].used) {
			/* 'dev' is saved here now for get_superblock() (which
			 * in turn is called by read_inode(), which in turn is
			 * called by iget(), which in turn is called by
			 * read_superblock) to be able to find the device.
			 */
			mount_table[n].dev = dev; 
			mount_table[n].used = 1;
			RESTORE_FLAGS(flags);
			return &mount_table[n];
		}
	}
	RESTORE_FLAGS(flags);

	printk("WARNING: %s(): mount-point table is full.\n", __FUNCTION__);
	return NULL;
}

void release_mount_point(struct mount *mt)
{
	memset_b(mt, 0, sizeof(struct mount));
}

struct mount * get_mount_point(struct inode *i_target)
{
	int n;

	for(n = 0; n < NR_MOUNT_POINTS; n++) {
		if(mount_table[n].used) {
			if(S_ISDIR(i_target->i_mode)) {
				if(mount_table[n].sb.root == i_target) {
					return &mount_table[n];
				}
			}
			if(S_ISBLK(i_target->i_mode)) {
				if(mount_table[n].dev == i_target->rdev) {
					return &mount_table[n];
				}
			}
		}
	}
	return NULL;
}

struct superblock * get_superblock(__dev_t dev)
{
	int n;

	for(n = 0; n < NR_MOUNT_POINTS; n++) {
		if(mount_table[n].used && mount_table[n].dev == dev) {
			return &mount_table[n].sb;
		}
	}
	return NULL;
}

void sync_superblocks(__dev_t dev)
{
	struct superblock *sb;
	int n, errno;

	lock_resource(&sync_resource);
	for(n = 0; n < NR_MOUNT_POINTS; n++) {
		if(mount_table[n].used && (!dev || mount_table[n].dev == dev)) {
			sb = &mount_table[n].sb;
			if(sb->dirty && !(sb->flags & MS_RDONLY)) {
				if(sb->fsop && sb->fsop->write_superblock) {
					errno = sb->fsop->write_superblock(sb);
					if(errno) {
						printk("WARNING: %s(): I/O error on device %d,%d while syncing superblock.\n", __FUNCTION__, MAJOR(sb->dev), MINOR(sb->dev));
					}
				}
			}
		}
	}
	unlock_resource(&sync_resource);
}

/* pseudo-filesystems are only mountable by the kernel */
int kern_mount(__dev_t dev, struct filesystems *fs)
{
	struct mount *mt;

	if(!(mt = get_free_mount_point(dev))) {
		return -EBUSY;
	}

	if(fs->fsop->read_superblock(dev, &mt->sb)) {
		release_mount_point(mt);
		return -EINVAL;
	}

	mt->dev = dev;
	strcpy(mt->devname, "none");
	strcpy(mt->dirname, "none");
	mt->sb.dir = NULL;
	mt->fs = fs;
	fs->mt = mt;
	return 0;
}

int mount_root(void)
{
	struct filesystems *fs;
	struct mount *mt;

	/* FIXME: before trying to mount the filesystem, we should first
	 * check if '_rootdev' is a device successfully registered.
	 */

	if(!_rootdev) {
		PANIC("root device not defined.\n");
	}

	if(!(fs = get_filesystem(_rootfstype))) {
		printk("WARNING: %s(): '%s' is not a registered filesystem. Defaulting to 'minix'.\n", __FUNCTION__, _rootfstype);
		if(!(fs = get_filesystem("minix"))) {
			PANIC("minix filesystem is not registered!\n");
		}
	}

	if(!(mt = get_free_mount_point(_rootdev))) {
		PANIC("unable to get a free mount point.\n");
	}

	mt->sb.flags = MS_RDONLY;
	if(fs->fsop && fs->fsop->read_superblock) {
		if(fs->fsop->read_superblock(_rootdev, &mt->sb)) {
			PANIC("unable to mount root filesystem on %s.\n", _rootdevname);
		}
	}

	strcpy(mt->devname, "/dev/root");
	strcpy(mt->dirname, "/");
	mt->dev = _rootdev;
	mt->sb.root->mount_point = mt->sb.root;
	mt->sb.root->count++;
	mt->sb.dir = mt->sb.root;
	mt->sb.dir->count++;
	mt->fs = fs;

	current->root = mt->sb.root;
	current->root->count++;
	current->pwd = mt->sb.root;
	current->pwd->count++;
	iput(mt->sb.root);

	printk("mounted root device (%s filesystem) in readonly mode.\n", fs->name);
	return 0;
}

void mount_init(void)
{
	memset_b(mount_table, 0, mount_table_size);
}
