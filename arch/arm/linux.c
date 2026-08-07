/*
 * fiwix/arch/arm/linux.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_fdt.h>
#include <fiwix/arm_linux.h>
#include <fiwix/fcntl.h>
#include <fiwix/fs.h>
#include <fiwix/mm.h>
#include <fiwix/stat.h>
#include <fiwix/string.h>

#define ARM_ZIMAGE_HEADER_SIZE	0x30U
#define ARM_ZIMAGE_MAGIC_OFFSET	0x24U
#define ARM_ZIMAGE_START_OFFSET	0x28U
#define ARM_ZIMAGE_END_OFFSET	0x2CU
#define ARM_ZIMAGE_MAGIC	0x016F2818U

extern unsigned int arm_boot_dtb;

static unsigned int get32(const unsigned char *p)
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
		((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

int arm_linux_page_reserved(unsigned long address)
{
	if(address >= ARM_LINUX_IMAGE_ADDRESS &&
		address < ARM_LINUX_IMAGE_ADDRESS +
		ARM_LINUX_IMAGE_CAPACITY) {
		return 1;
	}
	return address >= ARM_LINUX_DTB_ADDRESS &&
		address < ARM_LINUX_DTB_ADDRESS + ARM_LINUX_DTB_CAPACITY;
}

int arm_linux_zimage_header_gate(const void *image, unsigned int size)
{
	const unsigned char *bytes;

	if(!image || size < ARM_ZIMAGE_HEADER_SIZE) {
		return -1;
	}
	bytes = (const unsigned char *)image;
	if(get32(bytes + ARM_ZIMAGE_MAGIC_OFFSET) != ARM_ZIMAGE_MAGIC ||
		get32(bytes + ARM_ZIMAGE_START_OFFSET) != 0 ||
		get32(bytes + ARM_ZIMAGE_END_OFFSET) != size) {
		return -1;
	}
	return 0;
}

static int copy_boot_dtb(void)
{
	const struct arm_fdt_info *info;
	unsigned char *destination;
	const unsigned char *source;
	unsigned int size;
	unsigned int n;

	info = arm_boot_fdt_info();
	size = arm_fdt_size((const void *)(unsigned long)arm_boot_dtb);
	if(!info || !arm_boot_fdt_selected() || !size ||
		size > ARM_LINUX_DTB_CAPACITY ||
		info->memory_pages <
		(ARM_LINUX_DTB_ADDRESS + ARM_LINUX_DTB_CAPACITY -
		PHYSICAL_MEMORY_BASE) / PAGE_SIZE) {
		return -1;
	}
	source = (const unsigned char *)(unsigned long)arm_boot_dtb;
	destination = (unsigned char *)(unsigned long)ARM_LINUX_DTB_ADDRESS;
	if(source < destination &&
		size > (unsigned int)(destination - source)) {
		for(n = size; n; n--) {
			destination[n - 1] = source[n - 1];
		}
	} else {
		for(n = 0; n < size; n++) {
			destination[n] = source[n];
		}
	}
	return 0;
}

static int load_linux_image(unsigned int *size)
{
	struct fd fd;
	struct inode *inode;
	unsigned char *destination;
	int opened;
	int result;

	inode = 0;
	opened = 0;
	result = -1;
	if(namei("/linux", &inode, 0, FOLLOW_LINKS) || !inode ||
		!S_ISREG(inode->i_mode) || !inode->i_size ||
		inode->i_size > ARM_LINUX_IMAGE_CAPACITY ||
		!inode->fsop || !inode->fsop->open ||
		!inode->fsop->read || !inode->fsop->close) {
		goto out;
	}
	memset_b(&fd, 0, sizeof(fd));
	fd.inode = inode;
	fd.flags = O_RDONLY;
	fd.count = 1;
	if(inode->fsop->open(inode, &fd)) {
		goto out;
	}
	opened = 1;
	destination =
		(unsigned char *)(unsigned long)ARM_LINUX_IMAGE_ADDRESS;
	if(inode->fsop->read(inode, &fd, (char *)destination,
		inode->i_size) != (int)inode->i_size) {
		goto out;
	}
	*size = inode->i_size;
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

int arm_linux_prepare(void)
{
	unsigned int size;

	if(copy_boot_dtb() < 0 || load_linux_image(&size) < 0 ||
		arm_linux_zimage_header_gate(
		(const void *)(unsigned long)ARM_LINUX_IMAGE_ADDRESS,
		size) < 0) {
		return -1;
	}
	return 0;
}

unsigned int arm_linux_image_entry(void)
{
	return ARM_LINUX_IMAGE_ADDRESS;
}

unsigned int arm_linux_dtb_entry(void)
{
	return ARM_LINUX_DTB_ADDRESS;
}
