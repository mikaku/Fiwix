/*
 * fiwix/include/fiwix/part.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_PART_H
#define _FIWIX_PART_H

#define PARTITION_BLOCK		0
#define NR_PARTITIONS		4	/* partitions in the MBR */
#define MBR_CODE_SIZE		446
#define ACTIVE_PART		0x80

struct hd_geometry {
	unsigned char heads;
	unsigned char sectors;
	unsigned short int cylinders;
	unsigned long int start;
};

struct partition {
	unsigned char status;
	unsigned char head;
	unsigned char sector;
	unsigned char cyl;
	unsigned char type;
	unsigned char endhead;
	unsigned char endsector;
	unsigned char endcyl;
	unsigned int startsect;
	unsigned int nr_sects;
};

int read_msdos_partition(__dev_t, struct partition *);

#endif /* _FIWIX_PART_H */
