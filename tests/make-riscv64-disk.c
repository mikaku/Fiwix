/* Build a deterministic 1 KiB-block, revision-0 ext2 smoke image. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE          1024U
#define BLOCK_COUNT         1024U
#define INODE_COUNT         128U
#define BLOCK_BITMAP_BLOCK  3U
#define INODE_BITMAP_BLOCK  4U
#define INODE_TABLE_BLOCK   5U
#define ROOT_DIR_BLOCK      21U
#define FILE_DATA_BLOCK     22U
#define ROOT_INODE          2U
#define FILE_INODE          12U
#define INODE_SIZE          128U

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

static void mark_bits(unsigned char *bitmap, unsigned int count)
{
	unsigned int n;

	for(n = 0; n < count; n++) {
		bitmap[n / 8] |= (unsigned char)(1U << (n % 8));
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
	unsigned int record_length, unsigned int type, const char *name)
{
	unsigned int length;

	length = (unsigned int)strlen(name);
	put32(entry + 0, inode);
	put16(entry + 4, record_length);
	entry[6] = (unsigned char)length;
	entry[7] = (unsigned char)type;
	memcpy(entry + 8, name, length);
}

int main(int argc, char **argv)
{
	static const char sector_marker[] =
		"Fiwix riscv64 virtio sector gate\n";
	static const char file_marker[] =
		"Fiwix riscv64 ext2 file gate passed\n";
	unsigned char *image;
	unsigned char *super;
	unsigned char *group;
	unsigned char *inodes;
	unsigned char *directory;
	FILE *output;
	size_t written;

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

	super = image + BLOCK_SIZE;
	put32(super + 0, INODE_COUNT);
	put32(super + 4, BLOCK_COUNT);
	put32(super + 12, BLOCK_COUNT - (FILE_DATA_BLOCK + 1));
	put32(super + 16, INODE_COUNT - FILE_INODE);
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
	put16(group + 12, BLOCK_COUNT - (FILE_DATA_BLOCK + 1));
	put16(group + 14, INODE_COUNT - FILE_INODE);
	put16(group + 16, 1);
	mark_bits(image + BLOCK_BITMAP_BLOCK * BLOCK_SIZE, FILE_DATA_BLOCK);
	mark_bits(image + INODE_BITMAP_BLOCK * BLOCK_SIZE, FILE_INODE);

	inodes = image + INODE_TABLE_BLOCK * BLOCK_SIZE;
	make_inode(inodes + (ROOT_INODE - 1) * INODE_SIZE,
		0x41ed, BLOCK_SIZE, 2, ROOT_DIR_BLOCK);
	make_inode(inodes + (FILE_INODE - 1) * INODE_SIZE,
		0x81a4, sizeof(file_marker) - 1, 1, FILE_DATA_BLOCK);

	directory = image + ROOT_DIR_BLOCK * BLOCK_SIZE;
	make_dirent(directory, ROOT_INODE, 12, 2, ".");
	make_dirent(directory + 12, ROOT_INODE, 12, 2, "..");
	make_dirent(directory + 24, FILE_INODE, BLOCK_SIZE - 24, 1,
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
