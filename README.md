Fiwix kernel v1.x.x
===================
Fiwix is an operating system kernel, written by Jordi Sanfeliu from scratch, based on the UNIX architecture and fully focused on being POSIX compatible. It is designed and developed mainly as a hobby OS but also for educational purposes, therefore the kernel code is kept as simple as possible.

It runs on the i386 (x86 32bit) hardware architecture and is compatible with a good base of existing GNU applications.

Features
--------
It offers many UNIX-like features:

- Mostly written in C language (Assembler only used in the needed parts).
- GRUB Multiboot Specification v1 compliant.
- Full 32bit protected mode non-preemptive kernel.
- For i386 processors and higher.
- Preemptive multitasking.
- Protected task environment (independent memory address per process).
- Interrupt and exception handling.
- POSIX-compliant (mostly).
- Process groups, sessions and job control.
- Interprocess communication with pipes and signals.
- BSD file locking mechanism (POSIX restricted to file and advisory only).
- Virtual memory management up to 4GB (1GB physical only and no swapping yet).
- Demand paging with Copy-On-Write feature.
- Linux 2.0 ABI system calls compatibility (mostly).
- ELF-386 executable format support (statically and dynamically linked).
- Round Robin based scheduler algorithm (no priorities yet).
- VFS abstraction layer.
- Minix v1 and v2 filesystem support.
- EXT2 filesystem support (read only) with 1KB, 2KB and 4KB block sizes.
- Linux-like PROC filesystem support (read only).
- PIPE pseudo-filesystem support.
- ISO9660 filesystem support with Rock Ridge extensions.
- RAMdisk device support.
- Initial RAMdisk (initrd) image support.
- SVGAlib based applications support.
- Keyboard driver with Linux keymaps support.
- Parallel port printer driver support.
- Basic implementation of a Pseudo-Random Number Generator.
- Floppy disk device driver and DMA management.
- IDE/ATA hard disk device driver.
- IDE/ATA ATAPI CDROM device driver.

Community
---------
- [Website](http://www.fiwix.org)
- [Wiki](https://github.com/mikaku/Fiwix/wiki)
- [Mailing List](https://lists.sourceforge.net/lists/listinfo/fiwix-general)
- [IRC](http://webchat.freenode.net/?channels=fiwix)

License
-------
Fiwix is free software licensed under the terms of the MIT License, see the LICENSE file for more details.

Credits
-------
Fiwix was created by [Jordi Sanfeliu](http://www.fibranet.cat).
You can contact me at [jordi@fibranet.cat](mailto:jordi@fibranet.cat).
