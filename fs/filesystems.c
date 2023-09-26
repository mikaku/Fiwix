/*
 * fiwix/fs/filesystems.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs_pipe.h>
#include <fiwix/fs_iso9660.h>
#include <fiwix/fs_proc.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct filesystems filesystems_table[NR_FILESYSTEMS];

int register_filesystem(const char *name, struct fs_operations *fsop)
{
	int n;
	__dev_t dev;

	for(n = 0; n < NR_FILESYSTEMS; n++) {
		if(filesystems_table[n].name) {
			if(strcmp(filesystems_table[n].name, name) == 0) {
				printk("WARNING: %s(): filesystem '%s' already registered!\n", __FUNCTION__, name);
				return 1;
			}
		}
		if(!filesystems_table[n].name) {
			filesystems_table[n].name = name;
			filesystems_table[n].fsop = fsop;
			if((fsop->flags & FSOP_KERN_MOUNT)) {
				dev = fsop->fsdev;
				return kern_mount(dev, &filesystems_table[n]);
			}
			return 0;
		}
	}
	printk("WARNING: %s(): filesystems table is full!\n", __FUNCTION__);
	return 1;
}

struct filesystems *get_filesystem(const char *name)
{
	int n;

	if(!name) {
		return NULL;
	}
	for(n = 0; n < NR_FILESYSTEMS; n++) {
		if(!filesystems_table[n].name) {
			continue;
		}
		if(strcmp(filesystems_table[n].name, name) == 0) {
			return &filesystems_table[n];
		}
	}
	return NULL;
}

void fs_init(void)
{
	memset_b(filesystems_table, 0, sizeof(filesystems_table));

#ifdef CONFIG_FS_MINIX
	if(minix_init()) {
		printk("%s(): unable to register 'minix' filesystem.\n", __FUNCTION__);
	}
#endif /* CONFIG_FS_MINIX */
	if(ext2_init()) {
		printk("%s(): unable to register 'ext2' filesystem.\n", __FUNCTION__);
	}
	if(pipefs_init()) {
		printk("%s(): unable to register 'pipefs' filesystem.\n", __FUNCTION__);
	}
	if(iso9660_init()) {
		printk("%s(): unable to register 'iso9660' filesystem.\n", __FUNCTION__);
	}
	if(procfs_init()) {
		printk("%s(): unable to register 'procfs' filesystem.\n", __FUNCTION__);
	}
}
