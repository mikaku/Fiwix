/*
 * fiwix/include/fiwix/fs_iso9660.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_FS_ISO9660_H
#define _FIWIX_FS_ISO9660_H

#include <fiwix/types.h>
#include <fiwix/limits.h>

#define ISO9660_SUPERBLOCK	16	/* ISO9660 superblock is in block 16 */
#define ISO9660_STANDARD_ID	"CD001"	/* standard identification */
#define ISO9660_SUPER_MAGIC	0x9660

#define ISO9660_VD_BOOT			0
#define ISO9660_VD_PRIMARY		1
#define ISO9660_VD_SUPPLEMENTARY	2
#define ISO9660_VD_PARTITION		3
#define ISO9660_VD_END			255

#define ISODCL(from, to)	((to - from) + 1)	/* descriptor length */

#define ISO9660_MAX_VD		10	/* maximum number of VD per CDROM */

/* inodes will have their directory block and their offset packed as follows:
 * 7FF7FF
 * \-/\-/
 *  ^  ^
 *  |  +----- offset value			(11bit entries)
 *  +-------- directory block where to find it 	(11bit entries)
 */
#define ISO9660_INODE_BITS	11	/* FIXME: it could be greater (16bit) */
#define ISO9660_INODE_MASK	0x7FF

#define ISO9660_FILE_NOTEXIST	0x01	/* file shouldn't exists for the user */
#define ISO9660_FILE_ISDIR	0x02	/* is a directory */
#define ISO9660_FILE_ISASSOC	0x04	/* associated file */
#define ISO9660_FILE_HASRECFMT	0x08	/* has a record format */
#define ISO9660_FILE_HASOWNER	0x10	/* has owner and group defined */
#define ISO9660_FILE_RESERVED5	0x20	/* reserved */
#define ISO9660_FILE_RESERVED6	0x40	/* reserved */
#define ISO9660_FILE_ISMULTIEXT	0x80	/* has more directory records */

#define SP_MAGIC1		0xBE
#define SP_MAGIC2		0xEF
#define GET_SIG(s1, s2)		((s1 << 8) | s2)

#define SL_CURRENT		0x02
#define SL_PARENT		0x04
#define SL_ROOT			0x08

#define TF_CREATION		0x01
#define TF_MODIFY		0x02
#define TF_ACCESS		0x04
#define TF_ATTRIBUTES		0x08
#define TF_BACKUP		0x10
#define TF_EXPIRATION		0x20
#define TF_EFFECTIVE		0x40
#define TF_LONG_FORM		0x80

#define NM_CONTINUE		0
#define NM_CURRENT		1
#define NM_PARENT		2

/* Primary Volume Descriptor */
struct iso9660_super_block {
	char type			[ISODCL(  1,   1)];	/* 7.1.1 */
	char id				[ISODCL(  2,   6)];
	char version			[ISODCL(  7,   7)];	/* 7.1.1 */
	char unused1			[ISODCL(  8,   8)];
	char system_id			[ISODCL(  9,  40)];	/* a-chars */
	char volume_id			[ISODCL( 41,  72)];	/* d-chars */
	char unused2			[ISODCL( 73,  80)];
	char volume_space_size		[ISODCL( 81,  88)];	/* 7.3.3 */
	char unused3			[ISODCL( 89, 120)];
	char volume_set_size		[ISODCL(121, 124)];	/* 7.2.3 */
	char volume_sequence_number	[ISODCL(125, 128)];	/* 7.2.3 */
	char logical_block_size		[ISODCL(129, 132)];	/* 7.2.3 */
	char path_table_size		[ISODCL(133, 140)];	/* 7.3.3 */
	char type_l_path_table		[ISODCL(141, 144)];	/* 7.3.1 */
	char opt_type_l_path_table	[ISODCL(145, 148)];	/* 7.3.1 */
	char type_m_path_table		[ISODCL(149, 152)];	/* 7.3.2 */
	char opt_type_m_path_table	[ISODCL(153, 156)];	/* 7.3.2 */
	char root_directory_record	[ISODCL(157, 190)];	/* 9.1 */
	char volume_set_id		[ISODCL(191, 318)];	/* d-chars */
	char publisher_id		[ISODCL(319, 446)];	/* a-chars */
	char preparer_id		[ISODCL(447, 574)];	/* a-chars */
	char application_id		[ISODCL(575, 702)];	/* a-chars */
	char copyright_file_id		[ISODCL(703, 739)];	/* 7.5 d-chars */
	char abstract_file_id		[ISODCL(740, 776)];	/* 7.5 d-chars */
	char bibliographic_file_id	[ISODCL(777, 813)];	/* 7.5 d-chars */
	char creation_date		[ISODCL(814, 830)];	/* 8.4.26.1 */
	char modification_date		[ISODCL(831, 847)];	/* 8.4.26.1 */
	char expiration_date		[ISODCL(848, 864)];	/* 8.4.26.1 */
	char effective_date		[ISODCL(865, 881)];	/* 8.4.26.1 */
	char file_structure_version	[ISODCL(882, 882)];
	char unused4			[ISODCL(883, 883)];
	char application_data		[ISODCL(884, 1395)];
	char unused5			[ISODCL(1396, 2048)];
};

struct iso9660_directory_record
{
	char length			[ISODCL( 1,  1)];	/* 7.1.1 */
	char ext_attr_length		[ISODCL( 2,  2)];	/* 7.1.1 */
	char extent			[ISODCL( 3, 10)];	/* 7.3.3 */
	char size			[ISODCL(11, 18)];	/* 7.3.3 */
	char date			[ISODCL(19, 25)];	/* 7 by 7.1.1 */
	char flags			[ISODCL(26, 26)];
	char file_unit_size		[ISODCL(27, 27)];	/* 7.1.1 */
	char interleave			[ISODCL(28, 28)];	/* 7.1.1 */
	char volume_sequence_number	[ISODCL(29, 32)];	/* 7.2.3 */
	char name_len			[ISODCL(33, 33)];	/* 7.1.1 */
	char name[0];
};

struct iso9660_pathtable_record
{
	char length			[ISODCL( 1,  1)];	/* 7.1.1 */
	char ext_attr_length		[ISODCL( 2,  2)];	/* 7.1.1 */
	char extent			[ISODCL( 3,  6)];	/* 7.3 */
	char parent			[ISODCL( 7,  8)];	/* 7.2 */
	char name[0];
};

struct susp_sp {
	unsigned char magic[2];
	char len_skip;
};

struct susp_ce {
	char block[8];
	char offset[8];
	char size[8];
};

struct susp_er {
	char len_id;
	char len_des;
	char len_src;
	char ext_ver;
	char data[0];
};

struct rrip_px {
	char mode[8];
	char nlink[8];
	char uid[8];
	char gid[8];
	char sn[8];
};

struct rrip_pn {
	char dev_h[8];
	char dev_l[8];
};

struct rrip_sl_component {
	unsigned char flags;
	unsigned char len;
	char name[0];
};

struct rrip_sl {
	unsigned char flags;
	struct rrip_sl_component area;
};

struct rrip_nm {
	unsigned char flags;
	char name[0];
};

struct rrip_tf_timestamp {
	char time[7];		/* assumes LONG_FORM bit always set to zero */
};

struct rrip_tf {
	char flags;
	struct rrip_tf_timestamp times[0];
};

struct susp_rrip {
	char signature[2];
	unsigned char len;
	unsigned char version;
	union {
		struct susp_sp sp;
		struct susp_ce ce;
		struct susp_er er;
		struct rrip_px px;
		struct rrip_pn pn;
		struct rrip_sl sl;
		struct rrip_nm nm;
		struct rrip_tf tf;
	} u;
};

struct iso9660_inode {
	__blk_t i_extent;
	struct inode *i_parent;		/* inode of its parent directory */
};

struct iso9660_sb_info {
	__u32 s_root_inode;
	char *pathtable_raw;
	struct iso9660_pathtable_record **pathtable;
	int paths;
	unsigned char rrip;
	struct iso9660_super_block *sb;
};

#endif /* _FIWIX_FS_ISO9660_H */
