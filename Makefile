# fiwix/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

TOPDIR := $(shell if [ "$$PWD" != "" ] ; then echo $$PWD ; else pwd ; fi)
INCLUDE = $(TOPDIR)/include
TMPFILE := $(shell mktemp)

ARCH = -m32
CPU = -march=i386
LANG = -std=c89

CC = $(CROSS_COMPILE)gcc $(ARCH) $(CPU) $(LANG) -D__KERNEL__ #-D__DEBUG__
LD = $(CROSS_COMPILE)ld
CPP = $(CROSS_COMPILE)cpp -P -I$(INCLUDE)
LIBGCC := $(shell dirname `$(CC) -print-libgcc-file-name`)

CFLAGS = -I$(INCLUDE) -O2 -fno-pie -fno-common -ffreestanding -Wall -Wstrict-prototypes $(CONFFLAGS) #-Wextra -Wno-unused-parameter
LDFLAGS = -m elf_i386 -nostartfiles -nostdlib -nodefaultlibs -nostdinc

DIRS =	kernel \
	kernel/syscalls \
	mm \
	fs \
	drivers/char \
	drivers/block \
	drivers/pci \
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
	drivers/pci/*.o \
	drivers/video/*.o \
	lib/*.o

export CC LD CFLAGS LDFLAGS INCLUDE

all:
	@echo "#define UTS_VERSION \"`date`\"" > include/fiwix/version.h
	@for n in $(DIRS) ; do (cd $$n ; $(MAKE)) || exit ; done
	$(CPP) $(CONFFLAGS) fiwix.ld > $(TMPFILE)
	$(LD) -N -T $(TMPFILE) $(LDFLAGS) $(OBJS) -L$(LIBGCC) -lgcc -o fiwix
	rm -f $(TMPFILE)
	nm fiwix | sort | gzip -9c > System.map.gz

clean:
	@for n in $(DIRS) ; do (cd $$n ; $(MAKE) clean) ; done
	rm -f *.o fiwix System.map.gz

