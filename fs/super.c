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
#include <fiwix/mm.h>

struct mount *mount_table = NULL;
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

struct mount *add_mount_point(__dev_t dev, const char *devname, const char *dirname)
{
	unsigned long int flags;
	struct mount *mp;

	if(kstat.mount_points + 1 > NR_MOUNT_POINTS) {
		printk("WARNING: tried to excedeed NR_MOUNT_POINTS (%d).\n", NR_MOUNT_POINTS);
		return NULL;
	}

	if(!(mp = (struct mount *)kmalloc2(sizeof(struct mount)))) {
		return NULL;
	}
	memset_b(mp, 0, sizeof(struct mount));

	if(!(mp->devname = (char *)kmalloc2(strlen(devname) + 1))) {
		kfree2((unsigned int)mp);
		return NULL;
	}
	if(!(mp->dirname = (char *)kmalloc2(strlen(dirname) + 1))) {
		kfree2((unsigned int)mp->devname);
		kfree2((unsigned int)mp);
		return NULL;
	}

	SAVE_FLAGS(flags); CLI();
	if(!mount_table) {
		mount_table = mp;
	} else {
		mp->prev = mount_table->prev;
		mount_table->prev->next = mp;
	}
	mount_table->prev = mp;
	RESTORE_FLAGS(flags);

	mp->dev = dev;
	strcpy(mp->devname, devname);
	strcpy(mp->dirname, dirname);
	kstat.mount_points++;
	return mp;
}

void del_mount_point(struct mount *mp)
{
	unsigned long int flags;
	struct mount *tmp;

	tmp = mp;

	if(!mp->next && !mp->prev) {
		printk("WARNING: %s(): trying to umount an unexistent mount point (%x, '%s', '%s').\n", __FUNCTION__, mp->dev, mp->devname, mp->dirname);
		return;
	}

	SAVE_FLAGS(flags); CLI();
	if(mp->next) {
		mp->next->prev = mp->prev;
	}
	if(mp->prev) {
		if(mp != mount_table) {
			mp->prev->next = mp->next;
		}
	}
	if(!mp->next) {
		mount_table->prev = mp->prev;
	}
	if(mp == mount_table) {
		mount_table = mp->next;
	}
	RESTORE_FLAGS(flags);

	kfree2((unsigned int)tmp->devname);
	kfree2((unsigned int)tmp->dirname);
	kfree2((unsigned int)tmp);
	kstat.mount_points--;
}

struct mount *get_mount_point(struct inode *i)
{
	struct mount *mp;

	mp = mount_table;

	while(mp) {
		if(S_ISDIR(i->i_mode)) {
			if(mp->sb.root == i) {
				return mp;
			}
		}
		if(S_ISBLK(i->i_mode)) {
			if(mp->dev == i->rdev) {
				return mp;
			}
		}
		mp = mp->next;
	}

	return NULL;
}

struct superblock *get_superblock(__dev_t dev)
{
	struct mount *mp;

	mp = mount_table;

	while(mp) {
		if(mp->dev == dev) {
			return &mp->sb;
		}
		mp = mp->next;
	}
	return NULL;
}

void sync_superblocks(__dev_t dev)
{
	struct superblock *sb;
	struct mount *mp;
	int errno;

	mp = mount_table;

	lock_resource(&sync_resource);
	while(mp) {
		if(mp->dev == dev) {
			sb = &mp->sb;
			if(sb->dirty && !(sb->flags & MS_RDONLY)) {
				if(sb->fsop && sb->fsop->write_superblock) {
					errno = sb->fsop->write_superblock(sb);
					if(errno) {
						printk("WARNING: %s(): I/O error on device %d,%d while syncing superblock.\n", __FUNCTION__, MAJOR(sb->dev), MINOR(sb->dev));
					}
				}
			}
		}
		mp = mp->next;
	}
	unlock_resource(&sync_resource);
}

/* pseudo-filesystems are only mountable by the kernel */
int kern_mount(__dev_t dev, struct filesystems *fs)
{
	struct mount *mp;

	if(!(mp = add_mount_point(dev, "none", "none"))) {
		return -EBUSY;
	}

	if(fs->fsop->read_superblock(dev, &mp->sb)) {
		del_mount_point(mp);
		return -EINVAL;
	}

	mp->sb.dir = NULL;
	mp->fs = fs;
	fs->mp = mp;
	return 0;
}

int mount_root(void)
{
	struct filesystems *fs;
	struct mount *mp;

	/*
	 * FIXME: before trying to mount the filesystem, we should first
	 * check if '_rootdev' is a device successfully registered.
	 */

	if(!kparm_rootdev) {
		PANIC("root device not defined.\n");
	}

	if(!(fs = get_filesystem(kparm_rootfstype))) {
		printk("WARNING: %s(): '%s' is not a registered filesystem. Defaulting to 'minix'.\n", __FUNCTION__, kparm_rootfstype);
		if(!(fs = get_filesystem("minix"))) {
			PANIC("minix filesystem is not registered!\n");
		}
	}

	if(!(mp = add_mount_point(kparm_rootdev, "/dev/root", "/"))) {
		PANIC("unable to get a free mount point.\n");
	}

	if(kparm_ro) {
		mp->sb.flags = MS_RDONLY;
	}
	if(fs->fsop && fs->fsop->read_superblock) {
		if(fs->fsop->read_superblock(kparm_rootdev, &mp->sb)) {
			PANIC("unable to mount root filesystem on %s.\n", kparm_rootdevname);
		}
	}

	mp->sb.root->mount_point = mp->sb.root;
	mp->sb.root->count++;
	mp->sb.dir = mp->sb.root;
	mp->sb.dir->count++;
	mp->fs = fs;

	current->root = mp->sb.root;
	current->root->count++;
	current->pwd = mp->sb.root;
	current->pwd->count++;
	iput(mp->sb.root);

	printk("mounted root device (%s filesystem)", fs->name);
	if(mp->sb.flags & MS_RDONLY) {
		printk(" in readonly mode");
	}
	printk(".\n");
	return 0;
}
