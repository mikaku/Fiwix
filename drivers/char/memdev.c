/*
 * fiwix/drivers/char/memdev.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/memdev.h>
#include <fiwix/devices.h>
#include <fiwix/fs.h>
#include <fiwix/errno.h>
#include <fiwix/mm.h>
#include <fiwix/mman.h>
#include <fiwix/bios.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

static struct fs_operations mem_driver_fsop = {
	0,
	0,

	mem_open,
	mem_close,
	mem_read,
	mem_write,
	NULL,			/* ioctl */
	mem_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	mem_mmap,
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lockup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct fs_operations kmem_driver_fsop = {
	0,
	0,

	kmem_open,
	kmem_close,
	kmem_read,
	kmem_write,
	NULL,			/* ioctl */
	kmem_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	mem_mmap,
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lockup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct fs_operations null_driver_fsop = {
	0,
	0,

	null_open,
	null_close,
	null_read,
	null_write,
	NULL,			/* ioctl */
	null_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lockup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct fs_operations port_driver_fsop = {
	0,
	0,

	port_open,
	port_close,
	port_read,
	port_write,
	NULL,			/* ioctl */
	port_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lockup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct fs_operations zero_driver_fsop = {
	0,
	0,

	zero_open,
	zero_close,
	zero_read,
	zero_write,
	NULL,			/* ioctl */
	zero_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lockup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct fs_operations full_driver_fsop = {
	0,
	0,

	full_open,
	full_close,
	full_read,
	full_write,
	NULL,			/* ioctl */
	full_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lockup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct fs_operations urandom_driver_fsop = {
	0,
	0,

	urandom_open,
	urandom_close,
	urandom_read,
	urandom_write,
	NULL,			/* ioctl */
	urandom_llseek,
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lockup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct fs_operations memdev_driver_fsop = {
	0,
	0,

	memdev_open,
	NULL,			/* close */
	NULL,			/* read */
	NULL,			/* write */
	NULL,			/* ioctl */
	NULL,			/* llseek */
	NULL,			/* readdir */
	NULL,			/* readdir64 */
	NULL,			/* mmap */
	NULL,			/* select */

	NULL,			/* readlink */
	NULL,			/* followlink */
	NULL,			/* bmap */
	NULL,			/* lockup */
	NULL,			/* rmdir */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* mknod */
	NULL,			/* truncate */
	NULL,			/* create */
	NULL,			/* rename */

	NULL,			/* read_block */
	NULL,			/* write_block */

	NULL,			/* read_inode */
	NULL,			/* write_inode */
	NULL,			/* ialloc */
	NULL,			/* ifree */
	NULL,			/* statfs */
	NULL,			/* read_superblock */
	NULL,			/* remount_fs */
	NULL,			/* write_superblock */
	NULL			/* release_superblock */
};

static struct device memdev_device = {
	"mem",
	MEMDEV_MAJOR,
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	0,
	NULL,
	&memdev_driver_fsop,
	NULL
};

int mem_open(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int mem_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int mem_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	unsigned int physical_memory;

	physical_memory = (kstat.physical_pages << PAGE_SHIFT);
	if(fd_table->offset >= physical_memory) {
		return 0;
	}
	count = MIN(count, physical_memory - fd_table->offset);
	memcpy_b(buffer, (void *)P2V((unsigned int)fd_table->offset), count);
	fd_table->offset += count;
	return count;
}

int mem_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	unsigned int physical_memory;

	physical_memory = (kstat.physical_pages << PAGE_SHIFT);
	if(fd_table->offset >= physical_memory) {
		return 0;
	}
	count = MIN(count, physical_memory - fd_table->offset);
	memcpy_b((void *)P2V((unsigned int)fd_table->offset), buffer, count);
	fd_table->offset += count;
	return count;
}

__loff_t mem_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}

int kmem_open(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int kmem_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int kmem_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	unsigned int physical_memory;

	physical_memory = P2V((kstat.physical_pages << PAGE_SHIFT));
	if(P2V(fd_table->offset + count) < physical_memory) {
		memcpy_b(buffer, (void *)P2V((unsigned int)fd_table->offset), count);
		fd_table->offset += count;
	} else {
		count = 0;
	}
	return count;
}

int kmem_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	unsigned int physical_memory;

	physical_memory = P2V((kstat.physical_pages << PAGE_SHIFT));
	if(P2V(fd_table->offset + count) < physical_memory) {
		memcpy_b((void *)P2V((unsigned int)fd_table->offset), buffer, count);
		fd_table->offset += count;
	} else {
		count = 0;
	}
	return count;
}

__loff_t kmem_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}

int null_open(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int null_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int null_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	return 0;
}

int null_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	return count;
}

__loff_t null_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}

int port_open(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int port_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int port_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	unsigned int n;

	if(fd_table->offset >= 65535) {
		return 0;
	}
	count = MIN(count, 65536 - fd_table->offset);
	for(n = fd_table->offset; n < (fd_table->offset + count); n++, buffer++) {
		*buffer = inport_b(n);
	}
	fd_table->offset += count;
	return count;
}

int port_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	unsigned int n;

	if(fd_table->offset >= 65535) {
		return 0;
	}
	count = MIN(count, 65536 - fd_table->offset);
	for(n = fd_table->offset; n < (fd_table->offset + count); n++, buffer++) {
		outport_b(n, *buffer);
	}
	fd_table->offset += count;
	return count;
}

__loff_t port_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}

int zero_open(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int zero_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int zero_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	memset_b(buffer, 0, count);
	return count;
}

int zero_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	return count;
}

__loff_t zero_llseek(struct inode *i, __loff_t offset)

{
	return offset;
}

int full_open(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int full_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int full_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	memset_b(buffer, 0, count);
	return count;
}

int full_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	return -ENOSPC;
}

__loff_t full_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}

int urandom_open(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int urandom_close(struct inode *i, struct fd *fd_table)
{
	return 0;
}

int urandom_read(struct inode *i, struct fd *fd_table, char *buffer, __size_t count)
{
	int n;

	for(n = 0; n < count; n++) {
		kstat.random_seed = kstat.random_seed * 1103515245 + 12345;
		*buffer = (char)(unsigned int)(kstat.random_seed / 65536) % 256;
		buffer++;
	}
	return count;
}

int urandom_write(struct inode *i, struct fd *fd_table, const char *buffer, __size_t count)
{
	return count;
}

__loff_t urandom_llseek(struct inode *i, __loff_t offset)
{
	return offset;
}

int memdev_open(struct inode *i, struct fd *fd_table)
{
	unsigned char minor;

	minor = MINOR(i->rdev);
	switch(minor) {
		case MEMDEV_MEM:
			i->fsop = &mem_driver_fsop;
			break;
		case MEMDEV_KMEM:
			i->fsop = &kmem_driver_fsop;
			break;
		case MEMDEV_NULL:
			i->fsop = &null_driver_fsop;
			break;
		case MEMDEV_PORT:
			i->fsop = &port_driver_fsop;
			break;
		case MEMDEV_ZERO:
			i->fsop = &zero_driver_fsop;
			break;
		case MEMDEV_FULL:
			i->fsop = &full_driver_fsop;
			break;
		case MEMDEV_RANDOM:
			i->fsop = &urandom_driver_fsop;
			break;
		case MEMDEV_URANDOM:
			i->fsop = &urandom_driver_fsop;
			break;
		default:
			return -ENXIO;
	}
	return i->fsop->open(i, fd_table);
}

/*
 * This function maps a range of physical addresses marked as not available for
 * use in the BIOS memory map, like the video RAM.
 */
int mem_mmap(struct inode *i, struct vma *vma)
{
	unsigned int addr, length;

	length = (vma->end - vma->start) & PAGE_MASK;

	/* this breaks down the range in 4KB chunks */
	for(addr = 0; addr < length; addr += PAGE_SIZE) {
		/* map the page only if is NOT available in the BIOS map */
		if(!is_addr_in_bios_map(vma->offset + addr)) {
			if(!map_page(current, (vma->start + addr) & PAGE_MASK, (vma->offset + addr) & PAGE_MASK, PROT_READ | PROT_WRITE)) {
				return -ENOMEM;
			}
		} else {
			printk("ERROR: %s(): mapping AVAILABLE pages in BIOS memory map isn't supported.\n", __FUNCTION__);
			printk("\tinvalid mapping: 0x%08x -> 0x%08x\n", (vma->start + addr) & PAGE_MASK, (vma->offset + addr) & PAGE_MASK);
			return -EAGAIN;
		}
	}
	invalidate_tlb();
	return 0;
}

void memdev_init(void)
{
	SET_MINOR(memdev_device.minors, MEMDEV_MEM);
	SET_MINOR(memdev_device.minors, MEMDEV_KMEM);
	SET_MINOR(memdev_device.minors, MEMDEV_NULL);
	SET_MINOR(memdev_device.minors, MEMDEV_PORT);
	SET_MINOR(memdev_device.minors, MEMDEV_ZERO);
	SET_MINOR(memdev_device.minors, MEMDEV_FULL);
	SET_MINOR(memdev_device.minors, MEMDEV_RANDOM);
	SET_MINOR(memdev_device.minors, MEMDEV_URANDOM);

	if(register_device(CHR_DEV, &memdev_device)) {
		printk("ERROR: %s(): unable to register memory devices.\n", __FUNCTION__);
		return;
	}
}
