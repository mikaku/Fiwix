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
TCC ?= tcc

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

CC_DRIVER = $(CROSS_COMPILE)$(CCEXE)
ifeq ($(CCEXE),tcc)
CC_DRIVER = $(TCC)
endif
CC = $(CC_DRIVER) $(ARCH) $(CPU) $(LANG) -D__KERNEL__ $(ARCH_DEFINES) $(CONFFLAGS) #-D__DEBUG__
AS = $(CROSS_COMPILE)as
ASFLAGS = $(ARCH)
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
ifeq ($(TARGET_ARCH),i386)
LD = $(CROSS_COMPILE)$(CCEXE) $(ARCH)
LDFLAGS = -static -nostdlib -nostdinc
# If you define CONFIG_VM_SPLIT22 this should be 0x80100000: make CCEXE="tcc" TEXTADDR="0x80100000"
TEXTADDR = 0xC0100000
endif
ifeq ($(TARGET_ARCH),riscv64)
LD = $(CROSS_COMPILE)ld
NM = $(CROSS_COMPILE)nm
LDFLAGS = -m elf64lriscv --no-warn-mismatch
TCC_PRIVATE_DIR ?= $(shell $(TCC) -vv 2>&1 | awk '/^install:/{print $$2; exit}')
TCC_LIBTCC1 ?= $(TCC_PRIVATE_DIR)/libtcc1.a
endif
endif


export CC AS ASFLAGS LD CFLAGS LDFLAGS INCLUDE

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
ifeq ($(TARGET_ARCH),i386)
	$(LD) -Wl,-Ttext=$(TEXTADDR) $(LDFLAGS) $(OBJS) -o fiwix
endif
ifeq ($(TARGET_ARCH),riscv64)
	$(LD) -T $(KERNEL_LDSCRIPT) $(LDFLAGS) $(OBJS) $(TCC_LIBTCC1) -o fiwix
	$(NM) fiwix | sort | gzip -9c > System.map.gz
endif
endif

clean:
	@for n in $(DIRS) ; do (cd $$n ; $(MAKE) clean) ; done
	rm -f *.o fiwix System.map.gz

test-riscv64: all
	@test "$(TARGET_ARCH)" = riscv64 || { echo "test-riscv64 requires TARGET_ARCH=riscv64" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" tests/riscv64-smoke.sh ./fiwix

test-riscv64-large-image: all
	@test "$(TARGET_ARCH)" = riscv64 || { echo "test-riscv64-large-image requires TARGET_ARCH=riscv64" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" tests/riscv64-large-image-smoke.sh ./fiwix

test-riscv64-linux: all
	@test "$(TARGET_ARCH)" = riscv64 || { echo "test-riscv64-linux requires TARGET_ARCH=riscv64" >&2; exit 1; }
	@test -n "$(LINUX_IMAGE)" || { echo "test-riscv64-linux requires LINUX_IMAGE=/path/to/Image" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" tests/riscv64-linux-smoke.sh ./fiwix

test-riscv64-tcc:
	@test "$(TARGET_ARCH)" = riscv64 || { echo "test-riscv64-tcc requires TARGET_ARCH=riscv64" >&2; exit 1; }
	$(MAKE) TARGET_ARCH=riscv64 CROSS_COMPILE="$(CROSS_COMPILE)" CCEXE=tcc TCC="$(TCC)" clean
	$(MAKE) TARGET_ARCH=riscv64 CROSS_COMPILE="$(CROSS_COMPILE)" CCEXE=tcc TCC="$(TCC)" QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" test-riscv64

test-riscv64-generic-compile:
	@test "$(TARGET_ARCH)" = riscv64 || { echo "test-riscv64-generic-compile requires TARGET_ARCH=riscv64" >&2; exit 1; }
	GENERIC_CC="$(CROSS_COMPILE)gcc" tests/riscv64-generic-compile.sh

.PHONY: all clean test-riscv64 test-riscv64-large-image test-riscv64-linux test-riscv64-tcc test-riscv64-generic-compile
