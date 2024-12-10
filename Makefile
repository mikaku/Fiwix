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

# CCEXE can be overridden at the command line. For example: make CCEXE="tcc"
# To use tcc see docs/tcc.txt
CCEXE=gcc

CC = $(CROSS_COMPILE)$(CCEXE) $(ARCH) $(CPU) $(LANG) -D__KERNEL__ $(CONFFLAGS) #-D__DEBUG__
CFLAGS = -I$(INCLUDE) -O2 -fno-pie -fno-common -ffreestanding -Wall -Wstrict-prototypes #-Wextra -Wno-unused-parameter

ifeq ($(CCEXE),gcc)
LD = $(CROSS_COMPILE)ld
CPP = $(CROSS_COMPILE)cpp -P -I$(INCLUDE)
LIBGCC := -L$(shell dirname `$(CC) -print-libgcc-file-name`) -lgcc
LDFLAGS = -m elf_i386
endif

ifeq ($(CCEXE),tcc)
LD = $(CROSS_COMPILE)$(CCEXE) $(ARCH)
LDFLAGS = -static -nostdlib -nostdinc
# If you define CONFIG_VM_SPLIT22 this should be 0x80100000: make CCEXE="tcc" TEXTADDR="0x80100000"
TEXTADDR = 0xC0100000
endif


DIRS =	kernel \
	kernel/syscalls \
	mm \
	fs \
	drivers/char \
	drivers/block \
	drivers/pci \
	drivers/video \
	net \
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
	fs/sockfs/*.o \
	drivers/char/*.o \
	drivers/block/*.o \
	drivers/pci/*.o \
	drivers/video/*.o \
	net/*.o \
	lib/*.o

export CC LD CFLAGS LDFLAGS INCLUDE

all:
	@echo "#define UTS_VERSION \"`date -u`\"" > include/fiwix/version.h
	@for n in $(DIRS) ; do (cd $$n ; $(MAKE)) || exit ; done
ifeq ($(CCEXE),gcc)
	$(CPP) $(CONFFLAGS) fiwix.ld > $(TMPFILE)
	$(LD) -N -T $(TMPFILE) $(LDFLAGS) $(OBJS) $(LIBGCC) -o fiwix
	rm -f $(TMPFILE)
	nm fiwix | sort | gzip -9c > System.map.gz
endif
ifeq ($(CCEXE),tcc)
	$(LD) -Wl,-Ttext=$(TEXTADDR) $(LDFLAGS) $(OBJS) -o fiwix
endif

clean:
	@for n in $(DIRS) ; do (cd $$n ; $(MAKE) clean) ; done
	rm -f *.o fiwix System.map.gz

