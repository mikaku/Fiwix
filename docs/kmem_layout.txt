Fiwix kernel memory address space on the i386 architecture
==========================================================

	+-------------------+ \
	| page_table        | |
	+-------------------+ |
	+-------------------+ |
	| page_hash_table   | |
	+-------------------+ | different
	+-------------------+ |
	| RAMdisk drives:   | | preallocated
	|   - initrd        | |
	|   - all-purpose   | | areas
	|   - kexec         | |
	+-------------------+ | of
	+-------------------+ |
	| fd_table          | | contiguous
	+-------------------+ |
	+-------------------+ | memory
	| inode_hash_table  | |
	+-------------------+ | spaces
	+-------------------+ |
	| buffer_hash_table | |
	+-------------------+ |
	+-------------------+ |
	| proc_table        | |
	+-------------------+ /
	+-------------------+
	| kpage_table       | kernel Page Tables
	+-------------------+
	+-------------------+
	| kpage_dir         | kernel Page Directory
	+-------------------+
	+-------------------+
	| initrd (optional) | allocated by GRUB or Multiboot
	+-------------------+
	+-------------------+
	| .bss section      |
	+-------------------+
	| .data section     |
	+-------------------+
	| .text section     | kernel binary (fiwix)
	+-------------------+
0x00100000
...
	+-------------------+
	| 4KB kpage_dir     | this page will be reused once the definitive
	| only used at boot | kpage_dir is installed
	+-------------------+
0x00050000
...
0x00010000
	+-------------------+
	| kernel stack (4k) | later used as stack for idle process
	+-------------------+
...
0x00000000
