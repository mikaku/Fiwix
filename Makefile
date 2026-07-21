# fiwix/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

TOPDIR := $(shell if [ "$$PWD" != "" ] ; then echo $$PWD ; else pwd ; fi)
INCLUDE = $(TOPDIR)/include
TMPFILE := $(shell mktemp)
LANG = -std=c89
TARGET_ARCH ?= i386
QEMU ?= qemu-system-riscv64
TIMEOUT ?= 10
ARCH_DEFINES =

# CCEXE can be overridden at the command line. For example: make CCEXE="tcc"
# To use tcc see docs/tcc.txt
CCEXE=gcc

ifeq ($(TARGET_ARCH),i386)
ARCH = -m32
CPU = -march=i386
KERNEL_LDSCRIPT = fiwix.ld
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
	fs/devpts/*.o \
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
endif

ifeq ($(TARGET_ARCH),riscv64)
ARCH = -march=rv64ima_zicsr_zifencei -mabi=lp64
CPU = -mcmodel=medany -msmall-data-limit=0
ARCH_DEFINES = -DCONFIG_ARCH_RISCV64
KERNEL_LDSCRIPT = arch/riscv64/fiwix.ld
DIRS = arch/riscv64
OBJS = arch/riscv64/*.o
endif

ifeq ($(filter $(TARGET_ARCH),i386 riscv64),)
$(error unsupported TARGET_ARCH '$(TARGET_ARCH)'; expected i386 or riscv64)
endif

CC = $(CROSS_COMPILE)$(CCEXE) $(ARCH) $(CPU) $(LANG) -D__KERNEL__ $(ARCH_DEFINES) $(CONFFLAGS) #-D__DEBUG__
CFLAGS = -I$(INCLUDE) -O2 -fno-pie -fno-pic -fno-common -fno-stack-protector -ffreestanding -Wall -Wstrict-prototypes #-Wextra -Wno-unused-parameter

ifeq ($(CCEXE),gcc)
LD = $(CROSS_COMPILE)ld
CPP = $(CROSS_COMPILE)cpp -P -I$(INCLUDE)
NM = $(CROSS_COMPILE)nm
LIBGCC := -L$(shell dirname `$(CC) -print-libgcc-file-name`) -lgcc
ifeq ($(TARGET_ARCH),i386)
LDFLAGS = -N -m elf_i386
endif
ifeq ($(TARGET_ARCH),riscv64)
LDFLAGS = -m elf64lriscv
endif
endif

ifeq ($(CCEXE),tcc)
ifneq ($(TARGET_ARCH),i386)
$(error CCEXE=tcc is not implemented for TARGET_ARCH '$(TARGET_ARCH)' yet)
endif
LD = $(CROSS_COMPILE)$(CCEXE) $(ARCH)
LDFLAGS = -static -nostdlib -nostdinc
# If you define CONFIG_VM_SPLIT22 this should be 0x80100000: make CCEXE="tcc" TEXTADDR="0x80100000"
TEXTADDR = 0xC0100000
endif


export CC LD CFLAGS LDFLAGS INCLUDE

all:
ifeq ($(TARGET_ARCH),i386)
	@echo "#define UTS_VERSION \"`date -u`\"" > include/fiwix/version.h
endif
	@for n in $(DIRS) ; do (cd $$n ; $(MAKE)) || exit ; done
ifeq ($(CCEXE),gcc)
	$(CPP) $(ARCH_DEFINES) $(CONFFLAGS) $(KERNEL_LDSCRIPT) > $(TMPFILE)
	$(LD) -T $(TMPFILE) $(LDFLAGS) $(OBJS) $(LIBGCC) -o fiwix
	rm -f $(TMPFILE)
	$(NM) fiwix | sort | gzip -9c > System.map.gz
endif
ifeq ($(CCEXE),tcc)
	$(LD) -Wl,-Ttext=$(TEXTADDR) $(LDFLAGS) $(OBJS) -o fiwix
endif

clean:
	@for n in $(DIRS) ; do (cd $$n ; $(MAKE) clean) ; done
	rm -f *.o fiwix System.map.gz

test-riscv64: all
	@test "$(TARGET_ARCH)" = riscv64 || { echo "test-riscv64 requires TARGET_ARCH=riscv64" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" tests/riscv64-smoke.sh ./fiwix

.PHONY: all clean test-riscv64
