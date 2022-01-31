# fiwix/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

TOPDIR := $(shell if [ "$$PWD" != "" ] ; then echo $$PWD ; else pwd ; fi)
INCLUDE = $(TOPDIR)/include

ARCH = -m32
CPU = -march=i386

CC = $(CROSS_COMPILE)gcc $(ARCH) $(CPU) -D__KERNEL__ #-D__DEBUG__
LD = $(CROSS_COMPILE)ld

CFLAGS = -I$(INCLUDE) -O2 -ffreestanding -Wall -Wstrict-prototypes #-Wextra
LDFLAGS = -m elf_i386 -nostartfiles -nostdlib -nodefaultlibs -nostdinc

DIRS =	kernel \
	kernel/syscalls \
	mm \
	fs \
	drivers/char \
	drivers/block \
	drivers/video \
	lib

OBJS = 	kernel/*.o \
	kernel/syscalls/*.o \
	mm/*.o \
	fs/*.o \
	fs/ext2/*.o \
	fs/iso9660/*.o \
	fs/minix/*.o \
	fs/pipefs/*.o \
	fs/procfs/*.o \
	drivers/char/*.o \
	drivers/block/*.o \
	drivers/video/*.o \
	lib/*.o

export CC LD CFLAGS LDFLAGS INCLUDE

all:
	@echo "#define UTS_VERSION \"`date`\"" > include/fiwix/version.h
	@for n in $(DIRS) ; do (cd $$n ; $(MAKE)) ; done
	$(LD) -N -T fiwix.ld $(LDFLAGS) $(OBJS) -o fiwix
	nm fiwix | sort | gzip -9c > System.map.gz

clean:
	@for n in $(DIRS) ; do (cd $$n ; $(MAKE) clean) ; done
	rm -f *.o fiwix System.map.gz

floppy:
	mkfs.ext2 -m 0 /dev/fd0
	mount -t ext2 /dev/fd0 /mnt/floppy
	@mkdir -p /mnt/floppy/boot/grub
	@echo "(fd0)   /dev/fd0" > /mnt/floppy/boot/grub/device.map
	@grub-install --root-directory=/mnt/floppy /dev/fd0
	@tools/MAKEBOOTDISK
	@cp -prf tools/etc/* /mnt/floppy/etc
	@cp fiwix /mnt/floppy/boot
	@cp System.map.gz /mnt/floppy/boot
	@cp tools/install.sh /mnt/floppy/sbin
	umount /mnt/floppy

floppy_update:
	mount -t ext2 /dev/fd0 /mnt/floppy
	cp fiwix /mnt/floppy/boot
	cp System.map.gz /mnt/floppy/boot
	umount /mnt/floppy

