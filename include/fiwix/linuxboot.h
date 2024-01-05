/*
 * include/fiwix/linuxboot.h
 *
 * Copyright 2024, Richard R. Masters.
 * Distributed under the terms of the Fiwix License.
 */

#define IS_VGA_VGA_COLOR	0x22

struct screen_info {
	__u8  unused1[6];
	__u8  mode;
	__u8  cols;
	__u8  unused2[2];
	__u16 ega_bx;
	__u8  unused3[2];
	__u8  lines;
	__u8  is_vga;
	__u16 points;
	__u8  unused4[46];
} __attribute__((packed));


struct setup_header {
	__u8  unused1[9];
	__u16 video_mode;
	__u8  unused2[10];
	__u16 version;
	__u8  unused3[8];
	__u8  loader_type;
	__u8  load_flags;
	__u8  unused4[6];
	__u32 initramfs_addr;
	__u32 initramfs_size;
	__u8  unused5[8];
	__u32 cmdline_addr;
	__u32 initramfs_addr_max;
	__u8  unused6[60];
} __attribute__((packed));


struct bios_mem_entry {
	__u64 addr;
	__u64 size;
	__u32 type;
} __attribute__((packed));


#define MAX_BIOS_MEM_ENTRIES 128

struct boot_params {
	struct screen_info screen_info;
	__u8   unused1[424];
	__u8   num_bios_mem_entries;
	__u8   unused2[8];
	struct setup_header hdr;
	__u8   unused3[100];
	struct bios_mem_entry bios_mem_table[MAX_BIOS_MEM_ENTRIES];
	__u8   unused4[816];
} __attribute__((packed));

/* End of Linux code */
