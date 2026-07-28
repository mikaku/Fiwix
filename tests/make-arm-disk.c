/*
 * Build a deterministic 1 KiB-block, revision-0 ext2 ARM smoke image.
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE		1024U
#define BLOCK_COUNT		8192U
#define INODE_COUNT		128U
#define BLOCK_BITMAP_BLOCK	3U
#define INODE_BITMAP_BLOCK	4U
#define INODE_TABLE_BLOCK	5U
#define ROOT_DIR_BLOCK		21U
#define FILE_DATA_BLOCK		22U
#define ROOT_INODE		2U
#define RESERVED_INODES		10U
#define FILE_INODE		12U
#define INODE_SIZE		128U

static void put16(unsigned char *p, unsigned int value)
{
	p[0] = (unsigned char)value;
	p[1] = (unsigned char)(value >> 8);
}

static void put32(unsigned char *p, unsigned long value)
{
	p[0] = (unsigned char)value;
	p[1] = (unsigned char)(value >> 8);
	p[2] = (unsigned char)(value >> 16);
	p[3] = (unsigned char)(value >> 24);
}

static void mark_bit(unsigned char *bitmap, unsigned int bit)
{
	bitmap[bit / 8] |= (unsigned char)(1U << (bit % 8));
}

static void mark_bits(unsigned char *bitmap, unsigned int count)
{
	unsigned int n;

	for(n = 0; n < count; n++) {
		mark_bit(bitmap, n);
	}
}

static void mark_padding(unsigned char *bitmap, unsigned int valid_bits)
{
	unsigned int n;

	for(n = valid_bits; n < BLOCK_SIZE * 8; n++) {
		mark_bit(bitmap, n);
	}
}

static void make_inode(unsigned char *inode, unsigned int mode,
	unsigned int size, unsigned int links, unsigned int block)
{
	put16(inode + 0, mode);
	put32(inode + 4, size);
	put16(inode + 26, links);
	put32(inode + 28, 2);
	put32(inode + 40, block);
}

static void make_dirent(unsigned char *entry, unsigned int inode,
	unsigned int record_length, const char *name)
{
	unsigned int length;

	length = (unsigned int)strlen(name);
	put32(entry + 0, inode);
	put16(entry + 4, record_length);
	entry[6] = (unsigned char)length;
	entry[7] = 0;
	memcpy(entry + 8, name, length);
}

int main(int argc, char **argv)
{
	static const char sector_marker[] =
		"Fiwix ARM virtio sector gate\n";
	static const char file_marker[] =
		"Fiwix ARM ext2 writable gate init\n";
	unsigned char *block_bitmap;
	unsigned char *directory;
	unsigned char *group;
	unsigned char *image;
	unsigned char *inode_bitmap;
	unsigned char *inodes;
	unsigned char *super;
	FILE *output;
	size_t written;
	unsigned int free_blocks;
	unsigned int free_inodes;

	if(argc != 2) {
		fprintf(stderr, "usage: %s OUTPUT\n", argv[0]);
		return 2;
	}
	image = (unsigned char *)calloc(BLOCK_COUNT, BLOCK_SIZE);
	if(!image) {
		fprintf(stderr, "unable to allocate disk image\n");
		return 2;
	}
	memcpy(image, sector_marker, sizeof(sector_marker) - 1);

	free_blocks = BLOCK_COUNT - (FILE_DATA_BLOCK + 1);
	free_inodes = INODE_COUNT - RESERVED_INODES - 1;
	super = image + BLOCK_SIZE;
	put32(super + 0, INODE_COUNT);
	put32(super + 4, BLOCK_COUNT);
	put32(super + 12, free_blocks);
	put32(super + 16, free_inodes);
	put32(super + 20, 1);
	put32(super + 24, 0);
	put32(super + 28, 0);
	put32(super + 32, 8192);
	put32(super + 36, 8192);
	put32(super + 40, INODE_COUNT);
	put16(super + 52, 0);
	put16(super + 54, 0xffff);
	put16(super + 56, 0xef53);
	put16(super + 58, 1);
	put16(super + 60, 1);
	put32(super + 72, 0);
	put32(super + 76, 0);

	group = image + 2 * BLOCK_SIZE;
	put32(group + 0, BLOCK_BITMAP_BLOCK);
	put32(group + 4, INODE_BITMAP_BLOCK);
	put32(group + 8, INODE_TABLE_BLOCK);
	put16(group + 12, free_blocks);
	put16(group + 14, free_inodes);
	put16(group + 16, 1);

	block_bitmap = image + BLOCK_BITMAP_BLOCK * BLOCK_SIZE;
	mark_bits(block_bitmap, FILE_DATA_BLOCK);
	mark_padding(block_bitmap, BLOCK_COUNT - 1);
	inode_bitmap = image + INODE_BITMAP_BLOCK * BLOCK_SIZE;
	mark_bits(inode_bitmap, RESERVED_INODES);
	mark_bit(inode_bitmap, FILE_INODE - 1);
	mark_padding(inode_bitmap, INODE_COUNT);

	inodes = image + INODE_TABLE_BLOCK * BLOCK_SIZE;
	make_inode(inodes + (ROOT_INODE - 1) * INODE_SIZE,
		0x41ed, BLOCK_SIZE, 2, ROOT_DIR_BLOCK);
	make_inode(inodes + (FILE_INODE - 1) * INODE_SIZE,
		0x81a4, sizeof(file_marker) - 1, 1, FILE_DATA_BLOCK);

	directory = image + ROOT_DIR_BLOCK * BLOCK_SIZE;
	make_dirent(directory, ROOT_INODE, 12, ".");
	make_dirent(directory + 12, ROOT_INODE, 12, "..");
	make_dirent(directory + 24, FILE_INODE, BLOCK_SIZE - 24,
		"bootstrap");
	memcpy(image + FILE_DATA_BLOCK * BLOCK_SIZE, file_marker,
		sizeof(file_marker) - 1);

	output = fopen(argv[1], "wb");
	if(!output) {
		perror(argv[1]);
		free(image);
		return 2;
	}
	written = fwrite(image, BLOCK_SIZE, BLOCK_COUNT, output);
	if(written != BLOCK_COUNT || fclose(output)) {
		fprintf(stderr, "unable to write %s\n", argv[1]);
		free(image);
		return 2;
	}
	free(image);
	return 0;
}
