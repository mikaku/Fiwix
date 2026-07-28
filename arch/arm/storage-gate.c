/*
 * fiwix/arch/arm/storage-gate.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_devices.h>
#include <fiwix/buffer.h>
#include <fiwix/fcntl.h>
#include <fiwix/filesystems.h>
#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/string.h>

static unsigned char arm_storage_sector[BPS] __attribute__((aligned(16)));

int arm_ext2_writable_gate(void)
{
	static const char initial[] =
		"Fiwix ARM ext2 writable gate init\n";
	static const char passed[] =
		"Fiwix ARM ext2 writable gate pass\n";
	char contents[sizeof(initial)];
	struct fd fd;
	struct inode *inode;
	__blk_t block;
	__dev_t device;
	int opened;
	int result;

	typedef char arm_storage_markers_must_match[
		(sizeof(initial) == sizeof(passed)) ? 1 : -1];

	(void)sizeof(arm_storage_markers_must_match);
	device = MKDEV(ARM_VIRTIO_BLK_MAJOR, ARM_VIRTIO_BLK_MINOR);
	inode = NULL;
	opened = 0;
	result = -1;
	if(namei("/bootstrap", &inode, NULL, FOLLOW_LINKS) ||
		!inode || !S_ISREG(inode->i_mode) ||
		!inode->fsop || !inode->fsop->open ||
		!inode->fsop->read || !inode->fsop->write) {
		goto out;
	}
	memset_b(&fd, 0, sizeof(fd));
	fd.inode = inode;
	fd.flags = O_RDWR;
	fd.count = 1;
	if(inode->fsop->open(inode, &fd)) {
		goto out;
	}
	opened = 1;
	memset_b(contents, 0, sizeof(contents));
	if(inode->fsop->read(inode, &fd, contents, sizeof(initial) - 1) !=
		sizeof(initial) - 1 ||
		memcmp(contents, initial, sizeof(initial) - 1)) {
		goto out;
	}
	block = bmap(inode, 0, FOR_READING);
	if(block <= 0) {
		goto out;
	}
	fd.offset = 0;
	if(inode->fsop->write(inode, &fd, passed, sizeof(passed) - 1) !=
		sizeof(passed) - 1) {
		goto out;
	}
	sync_superblocks(device);
	sync_inodes(device);
	sync_buffers(device);
	if(arm_virtio_read_sector(
		(unsigned long long)(unsigned int)block * 2ULL,
		arm_storage_sector) < 0 ||
		memcmp(arm_storage_sector, passed, sizeof(passed) - 1)) {
		goto out;
	}
	result = 0;

out:
	if(opened && inode->fsop->close(inode, &fd)) {
		result = -1;
	}
	if(inode) {
		iput(inode);
	}
	return result;
}
