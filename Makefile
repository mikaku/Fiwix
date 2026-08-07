# fiwix/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

TOPDIR := $(shell if [ "$$PWD" != "" ] ; then echo $$PWD ; else pwd ; fi)
INCLUDE = $(TOPDIR)/include
GENERATED_LDSCRIPT = .fiwix.generated.ld
LANG = -std=c89
TARGET_ARCH ?= i386
RISCV_MARCH ?= rv64ima_zicsr_zifencei
ARM_MARCH ?= armv7-a
ARM_CC_TARGET ?=
QEMU ?= qemu-system-riscv64
QEMU_ARM ?= qemu-system-arm
TIMEOUT ?= 10
HOSTCC ?= cc
GENERIC_FLOAT_ABI ?= soft
GENERIC_CC_INCLUDE ?=
GENERIC_RETAINED_STUBS ?=
GENERIC_WORKDIR ?=
ARCH_DEFINES =
CC_TARGET =

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
ARCH = -march=$(RISCV_MARCH) -mabi=lp64
CPU = -mcmodel=medany -msmall-data-limit=0
ARCH_DEFINES = -DCONFIG_ARCH_RISCV64
KERNEL_LDSCRIPT = arch/riscv64/fiwix.ld
DIRS = arch/riscv64
OBJS = arch/riscv64/*.o
endif

ifeq ($(TARGET_ARCH),arm)
ARCH = -march=$(ARM_MARCH) -mfloat-abi=soft
CPU = -marm -mno-unaligned-access
CC_TARGET = $(ARM_CC_TARGET)
ARCH_DEFINES = -DCONFIG_ARCH_ARM
KERNEL_LDSCRIPT = arch/arm/fiwix.ld
DIRS = arch/arm
OBJS = arch/arm/*.o
endif

ifeq ($(filter $(TARGET_ARCH),i386 riscv64 arm),)
$(error unsupported TARGET_ARCH '$(TARGET_ARCH)'; expected i386, riscv64, or arm)
endif

CC_DRIVER = $(CROSS_COMPILE)$(CCEXE)
ifeq ($(CCEXE),tcc)
CC_DRIVER = $(TCC)
endif
ifeq ($(CCEXE),clang)
CC_DRIVER = clang
endif
CC = $(CC_DRIVER) $(CC_TARGET) $(ARCH) $(CPU) $(LANG) -D__KERNEL__ $(ARCH_DEFINES) $(CONFFLAGS) #-D__DEBUG__
AS = $(CROSS_COMPILE)as
ASFLAGS = $(ARCH)
CFLAGS = -I$(INCLUDE) -O2 -fno-pie -fno-pic -fno-common -fno-stack-protector -ffreestanding -Wall -Wstrict-prototypes #-Wextra -Wno-unused-parameter

ifneq ($(filter $(CCEXE),gcc clang),)
LD = $(CROSS_COMPILE)ld
CPP = $(CROSS_COMPILE)cpp -P -I$(INCLUDE)
NM = $(CROSS_COMPILE)nm
OBJCOPY = $(CROSS_COMPILE)objcopy
LIBGCC = -L$(shell dirname `$(CC) -print-libgcc-file-name`) -lgcc
ifeq ($(TARGET_ARCH),i386)
LDFLAGS = -N -m elf_i386
endif
ifeq ($(TARGET_ARCH),riscv64)
LDFLAGS = -m elf64lriscv
endif
ifeq ($(TARGET_ARCH),arm)
LDFLAGS = -m armelf_linux_eabi
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
ifneq ($(filter $(CCEXE),gcc clang),)
	$(CPP) $(ARCH_DEFINES) $(CONFFLAGS) $(KERNEL_LDSCRIPT) > $(GENERATED_LDSCRIPT)
	$(LD) -T $(GENERATED_LDSCRIPT) $(LDFLAGS) $(OBJS) $(LIBGCC) -o fiwix
	rm -f $(GENERATED_LDSCRIPT)
	$(NM) fiwix | sort | gzip -9c > System.map.gz
ifeq ($(TARGET_ARCH),arm)
	$(OBJCOPY) -O binary fiwix fiwix-arm.bin
endif
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
	rm -f *.o fiwix fiwix-arm.bin fiwix-arm-generic \
		fiwix-arm-generic.bin fiwix-generic System.map.gz \
		$(GENERATED_LDSCRIPT)

test-riscv64: all
	@test "$(TARGET_ARCH)" = riscv64 || { echo "test-riscv64 requires TARGET_ARCH=riscv64" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" tests/riscv64-smoke.sh ./fiwix

test-arm-serial-line:
	tests/arm-serial-line-selftest.sh

test-arm: all test-arm-serial-line
	@test "$(TARGET_ARCH)" = arm || { echo "test-arm requires TARGET_ARCH=arm" >&2; exit 1; }
	HOSTCC="$(HOSTCC)" AS="$(AS)" NM="$(NM)" tests/arm-context.sh
	HOSTCC="$(HOSTCC)" tests/arm-vm-policy.sh
	HOSTCC="$(HOSTCC)" QEMU="$(QEMU_ARM)" tests/arm-fdt.sh
	HOSTCC="$(HOSTCC)" tests/arm-linux-image.sh
	HOSTCC="$(HOSTCC)" ARMCC="$(CC_DRIVER)" ARMCC_TARGET="$(CC_TARGET)" \
		tests/arm-process-roots.sh
	HOSTCC="$(HOSTCC)" ARMCC="$(CC_DRIVER)" ARMCC_TARGET="$(CC_TARGET)" \
		tests/arm-generic-memory.sh
	ARMCC="$(CC_DRIVER)" ARMCC_TARGET="$(CC_TARGET)" AS="$(AS)" NM="$(NM)" \
		OBJCOPY="$(OBJCOPY)" READELF="$(CROSS_COMPILE)readelf" \
		tests/arm-init-process.sh
	HOSTCC="$(HOSTCC)" tests/arm-elf32-plan.sh
	HOSTCC="$(HOSTCC)" ARMCC="$(CC_DRIVER)" ARMCC_TARGET="$(CC_TARGET)" \
		tests/arm-elf32-load.sh
	HOSTCC="$(HOSTCC)" ARMCC="$(CC_DRIVER)" ARMCC_TARGET="$(CC_TARGET)" \
		tests/arm-signal-frame.sh
	ARMCC="$(CC_DRIVER)" ARMCC_TARGET="$(CC_TARGET)" \
		tests/arm-signal-uapi.sh
	AS="$(AS)" LD="$(LD)" NM="$(NM)" OBJCOPY="$(OBJCOPY)" QEMU="$(QEMU_ARM)" \
		TIMEOUT="$(TIMEOUT)" tests/arm-generic-trap-runtime.sh
	HOSTCC="$(HOSTCC)" ARMCC="$(CC_DRIVER)" ARMCC_TARGET="$(CC_TARGET)" \
		tests/arm-generic-trap-policy.sh
	ARMCC="$(CC_DRIVER)" ARMCC_TARGET="$(CC_TARGET)" \
		tests/arm-generic-process-compile.sh
	HOSTCC="$(HOSTCC)" tests/arm-syscall-translation.sh
	OBJCOPY="$(OBJCOPY)" READELF="$(CROSS_COMPILE)readelf" \
		tests/arm-elf32.sh ./fiwix
	QEMU="$(QEMU_ARM)" TIMEOUT="$(TIMEOUT)" tests/arm-smoke.sh ./fiwix-arm.bin

test-arm-generic-compile:
	@test "$(TARGET_ARCH)" = arm || { echo "test-arm-generic-compile requires TARGET_ARCH=arm" >&2; exit 1; }
	GENERIC_CC="$(if $(GENERIC_CC),$(GENERIC_CC),$(CC_DRIVER))" \
		GENERIC_CC_TARGET="$(CC_TARGET)" \
		GENERIC_FLOAT_ABI="$(GENERIC_FLOAT_ABI)" \
		GENERIC_CC_INCLUDE="$(GENERIC_CC_INCLUDE)" \
		GENERIC_LD="$(CROSS_COMPILE)ld" \
		GENERIC_RETAINED_STUBS="$(GENERIC_RETAINED_STUBS)" \
		tests/arm-generic-compile.sh

arm-generic-image:
	@test "$(TARGET_ARCH)" = arm || { echo "arm-generic-image requires TARGET_ARCH=arm" >&2; exit 1; }
	GENERIC_CC="$(if $(GENERIC_CC),$(GENERIC_CC),$(CC_DRIVER))" \
		GENERIC_CC_TARGET="$(CC_TARGET)" \
		GENERIC_FLOAT_ABI="$(GENERIC_FLOAT_ABI)" \
		GENERIC_CC_INCLUDE="$(GENERIC_CC_INCLUDE)" \
		GENERIC_LD="$(CROSS_COMPILE)ld" \
		GENERIC_RUNTIME="$(GENERIC_RUNTIME)" \
		GENERIC_RETAINED_STUBS="$(GENERIC_RETAINED_STUBS)" \
		GENERIC_WORKDIR="$(GENERIC_WORKDIR)" \
		AS="$(AS)" NM="$(NM)" OBJCOPY="$(OBJCOPY)" \
		READELF="$(CROSS_COMPILE)readelf" \
		tests/arm-generic-image.sh

arm-generic-disk:
	$(MAKE) -C arch/arm HOSTCC="$(HOSTCC)" fixture/disk.img
	READELF="$(CROSS_COMPILE)readelf" NM="$(NM)" \
		tests/arm-generic-init.sh arch/arm/fixture/generic-init.elf

arm-linux-root-init:
	$(MAKE) -C arch/arm fixture/linux-root-init.elf

arm-linux-root-disk: arm-linux-root-init
	@test -n "$(LINUX_IMAGE)" || { echo "arm-linux-root-disk requires LINUX_IMAGE=/path/to/zImage" >&2; exit 1; }
	$(MAKE) -C arch/arm HOSTCC="$(HOSTCC)" \
		LINUX_IMAGE="$(LINUX_IMAGE)" fixture/linux-root-disk.img

test-arm-linux-root: TIMEOUT=60
test-arm-linux-root: arm-generic-image arm-linux-root-disk
	@test "$(TARGET_ARCH)" = arm || { echo "test-arm-linux-root requires TARGET_ARCH=arm" >&2; exit 1; }
	QEMU="$(QEMU_ARM)" TIMEOUT="$(TIMEOUT)" \
		tests/arm-linux-root-smoke.sh ./fiwix-arm-generic.bin \
		arch/arm/fixture/linux-root-disk.img

arm-linux-tcc-init:
	$(MAKE) -C arch/arm fixture/linux-tcc-init.elf

test-arm-linux-tcc: TIMEOUT=120
test-arm-linux-tcc: arm-generic-image-tcc arm-linux-tcc-init
	@test -n "$(LINUX_IMAGE)" || { echo "test-arm-linux-tcc requires LINUX_IMAGE=/path/to/zImage" >&2; exit 1; }
	@test -n "$(ARM_TCC)" || { echo "test-arm-linux-tcc requires ARM_TCC=/path/to/tcc-mes" >&2; exit 1; }
	QEMU="$(QEMU_ARM)" TIMEOUT="$(TIMEOUT)" ARM_TCC="$(ARM_TCC)" \
		LINUX_IMAGE="$(LINUX_IMAGE)" \
		tests/arm-linux-tcc-boot-smoke.sh ./fiwix-arm-generic.bin \
		arch/arm/fixture/linux-tcc-init.elf

arm-linux-tcc-seed-init:
	$(MAKE) -C arch/arm fixture/linux-tcc-seed-init.elf

test-arm-linux-tcc-seed: TIMEOUT=14400
test-arm-linux-tcc-seed: arm-generic-image-tcc arm-linux-tcc-seed-init
	@test -n "$(LINUX_IMAGE)" || { echo "test-arm-linux-tcc-seed requires LINUX_IMAGE=/path/to/zImage" >&2; exit 1; }
	@test -n "$(ARM_MES)" || { echo "test-arm-linux-tcc-seed requires ARM_MES=/path/to/mes-m2" >&2; exit 1; }
	@test -n "$(ARM_NYACC)" || { echo "test-arm-linux-tcc-seed requires ARM_NYACC=/path/to/nyacc" >&2; exit 1; }
	@test -n "$(ARM_M1)" || { echo "test-arm-linux-tcc-seed requires ARM_M1=/path/to/ARM/M1" >&2; exit 1; }
	@test -n "$(ARM_HEX2)" || { echo "test-arm-linux-tcc-seed requires ARM_HEX2=/path/to/ARM/hex2" >&2; exit 1; }
	@test -n "$(ARM_TCC_SOURCE)" || { echo "test-arm-linux-tcc-seed requires ARM_TCC_SOURCE=/path/to/tinycc-source" >&2; exit 1; }
	QEMU="$(QEMU_ARM)" TIMEOUT="$(TIMEOUT)" HANDOFF_LINUX=1 \
		LINUX_IMAGE="$(LINUX_IMAGE)" ARM_MES="$(ARM_MES)" \
		ARM_NYACC="$(ARM_NYACC)" ARM_M1="$(ARM_M1)" \
		ARM_HEX2="$(ARM_HEX2)" ARM_TCC_SOURCE="$(ARM_TCC_SOURCE)" \
		tests/arm-tcc-seed-boot-smoke.sh ./fiwix-arm-generic.bin \
		arch/arm/fixture/linux-tcc-seed-init.elf

arm-m2-pivot-init:
	$(MAKE) -C arch/arm fixture/m2-pivot-init.elf

test-arm-m2-pivot: TIMEOUT=300
test-arm-m2-pivot: arm-generic-image-tcc arm-m2-pivot-init
	@test -n "$(STAGE0_DIR)" || { echo "test-arm-m2-pivot requires STAGE0_DIR=/path/to/completed/arm-pivot" >&2; exit 1; }
	QEMU="$(QEMU_ARM)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		tests/arm-m2-pivot-boot-smoke.sh ./fiwix-arm-generic.bin \
		arch/arm/fixture/m2-pivot-init.elf

arm-mes-init:
	$(MAKE) -C arch/arm fixture/mes-init.elf

test-arm-mes: TIMEOUT=60
test-arm-mes: arm-generic-image-tcc arm-mes-init
	@test -n "$(ARM_MES)" || { echo "test-arm-mes requires ARM_MES=/path/to/mes-m2" >&2; exit 1; }
	QEMU="$(QEMU_ARM)" TIMEOUT="$(TIMEOUT)" ARM_MES="$(ARM_MES)" \
		tests/arm-mes-boot-smoke.sh ./fiwix-arm-generic.bin \
		arch/arm/fixture/mes-init.elf

arm-mescc-init:
	$(MAKE) -C arch/arm fixture/mescc-init.elf

test-arm-mescc: TIMEOUT=1800
test-arm-mescc: arm-generic-image-tcc arm-mescc-init
	@test -n "$(ARM_MES)" || { echo "test-arm-mescc requires ARM_MES=/path/to/mes-m2" >&2; exit 1; }
	@test -n "$(ARM_NYACC)" || { echo "test-arm-mescc requires ARM_NYACC=/path/to/nyacc" >&2; exit 1; }
	@test -n "$(ARM_M1)" || { echo "test-arm-mescc requires ARM_M1=/path/to/ARM/M1" >&2; exit 1; }
	@test -n "$(ARM_HEX2)" || { echo "test-arm-mescc requires ARM_HEX2=/path/to/ARM/hex2" >&2; exit 1; }
	QEMU="$(QEMU_ARM)" TIMEOUT="$(TIMEOUT)" ARM_MES="$(ARM_MES)" \
		ARM_NYACC="$(ARM_NYACC)" ARM_M1="$(ARM_M1)" \
		ARM_HEX2="$(ARM_HEX2)" tests/arm-mescc-boot-smoke.sh \
		./fiwix-arm-generic.bin arch/arm/fixture/mescc-init.elf

arm-tcc-seed-init:
	$(MAKE) -C arch/arm fixture/tcc-seed-init.elf

test-arm-tcc-seed: TIMEOUT=14400
test-arm-tcc-seed: arm-generic-image-tcc arm-tcc-seed-init
	@test -n "$(ARM_MES)" || { echo "test-arm-tcc-seed requires ARM_MES=/path/to/mes-m2" >&2; exit 1; }
	@test -n "$(ARM_NYACC)" || { echo "test-arm-tcc-seed requires ARM_NYACC=/path/to/nyacc" >&2; exit 1; }
	@test -n "$(ARM_M1)" || { echo "test-arm-tcc-seed requires ARM_M1=/path/to/ARM/M1" >&2; exit 1; }
	@test -n "$(ARM_HEX2)" || { echo "test-arm-tcc-seed requires ARM_HEX2=/path/to/ARM/hex2" >&2; exit 1; }
	@test -n "$(ARM_TCC_SOURCE)" || { echo "test-arm-tcc-seed requires ARM_TCC_SOURCE=/path/to/tinycc-source" >&2; exit 1; }
	QEMU="$(QEMU_ARM)" TIMEOUT="$(TIMEOUT)" ARM_MES="$(ARM_MES)" \
		ARM_NYACC="$(ARM_NYACC)" ARM_M1="$(ARM_M1)" \
		ARM_HEX2="$(ARM_HEX2)" ARM_TCC_SOURCE="$(ARM_TCC_SOURCE)" \
		tests/arm-tcc-seed-boot-smoke.sh ./fiwix-arm-generic.bin \
		arch/arm/fixture/tcc-seed-init.elf

arm-tcc-init:
	$(MAKE) -C arch/arm fixture/tcc-init.elf

test-arm-tcc: TIMEOUT=120
test-arm-tcc: arm-generic-image-tcc arm-tcc-init
	@test -n "$(ARM_TCC)" || { echo "test-arm-tcc requires ARM_TCC=/path/to/tcc-mes" >&2; exit 1; }
	QEMU="$(QEMU_ARM)" TIMEOUT="$(TIMEOUT)" ARM_TCC="$(ARM_TCC)" \
		tests/arm-tcc-boot-smoke.sh ./fiwix-arm-generic.bin \
		arch/arm/fixture/tcc-init.elf

arm-tcc-selfhost-init:
	$(MAKE) -C arch/arm fixture/tcc-selfhost-init.elf

test-arm-tcc-selfhost: TIMEOUT=300
test-arm-tcc-selfhost: arm-generic-image-tcc arm-tcc-selfhost-init
	@test -n "$(ARM_TCC)" || { echo "test-arm-tcc-selfhost requires ARM_TCC=/path/to/tcc-mes" >&2; exit 1; }
	@test -n "$(ARM_TCC_SOURCE)" || { echo "test-arm-tcc-selfhost requires ARM_TCC_SOURCE=/path/to/tinycc-source" >&2; exit 1; }
	QEMU="$(QEMU_ARM)" TIMEOUT="$(TIMEOUT)" ARM_TCC="$(ARM_TCC)" \
		ARM_TCC_SOURCE="$(ARM_TCC_SOURCE)" \
		tests/arm-tcc-selfhost-boot-smoke.sh ./fiwix-arm-generic.bin \
		arch/arm/fixture/tcc-selfhost-init.elf

arm-generic-image-tcc:
	@test "$(TARGET_ARCH)" = arm || { echo "arm-generic-image-tcc requires TARGET_ARCH=arm" >&2; exit 1; }
	@test -f "$(TCC_LIBTCC1)" || { echo "arm-generic-image-tcc requires TCC_LIBTCC1=/path/to/libtcc1.a" >&2; exit 1; }
	@test -f "$(TCC_INCLUDE)/stdarg.h" || { echo "arm-generic-image-tcc requires TCC_INCLUDE=/path/to/tcc/include" >&2; exit 1; }
	$(MAKE) TARGET_ARCH=arm CROSS_COMPILE="$(CROSS_COMPILE)" \
		GENERIC_CC="$(TCC)" GENERIC_FLOAT_ABI= \
		GENERIC_CC_INCLUDE="$(TCC_INCLUDE)" \
		GENERIC_RETAINED_STUBS="tests/arm-generic-tcc-stubs.expected" \
		GENERIC_WORKDIR=.generic-package-work \
		GENERIC_RUNTIME="$(TCC_LIBTCC1)" arm-generic-image

test-arm-generic-tcc: arm-generic-image-tcc arm-generic-disk
	QEMU="$(QEMU_ARM)" TIMEOUT="$(TIMEOUT)" \
		tests/arm-generic-boot-smoke.sh ./fiwix-arm-generic.bin \
		arch/arm/fixture/disk.img

test-arm-generic-boot: arm-generic-image arm-generic-disk
	QEMU="$(QEMU_ARM)" TIMEOUT="$(TIMEOUT)" \
		tests/arm-generic-boot-smoke.sh ./fiwix-arm-generic.bin \
		arch/arm/fixture/disk.img

test-riscv64-large-image: all
	@test "$(TARGET_ARCH)" = riscv64 || { echo "test-riscv64-large-image requires TARGET_ARCH=riscv64" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" tests/riscv64-large-image-smoke.sh ./fiwix

test-riscv64-linux: all
	@test "$(TARGET_ARCH)" = riscv64 || { echo "test-riscv64-linux requires TARGET_ARCH=riscv64" >&2; exit 1; }
	@test -n "$(LINUX_IMAGE)" || { echo "test-riscv64-linux requires LINUX_IMAGE=/path/to/Image" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" tests/riscv64-linux-smoke.sh ./fiwix

riscv64-linux-root-init:
	$(MAKE) -C arch/riscv64 fixture/linux-root-init.elf

riscv64-linux-kaem-init:
	$(MAKE) -C arch/riscv64 fixture/linux-kaem-init.elf

riscv64-linux-stage0-complete:
	$(MAKE) -C arch/riscv64 fixture/linux-stage0-complete.elf

riscv64-linux-root-disk: riscv64-linux-root-init
	@test -n "$(LINUX_IMAGE)" || { echo "riscv64-linux-root-disk requires LINUX_IMAGE=/path/to/Image" >&2; exit 1; }
	$(MAKE) -C arch/riscv64 LINUX_IMAGE="$(LINUX_IMAGE)" fixture/linux-root-disk.img

test-riscv64-linux-root: TIMEOUT=30
test-riscv64-linux-root: all riscv64-linux-root-disk
	@test "$(TARGET_ARCH)" = riscv64 || { echo "test-riscv64-linux-root requires TARGET_ARCH=riscv64" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" \
		tests/riscv64-linux-root-smoke.sh ./fiwix \
		arch/riscv64/fixture/linux-root-disk.img

test-riscv64-tcc:
	@test "$(TARGET_ARCH)" = riscv64 || { echo "test-riscv64-tcc requires TARGET_ARCH=riscv64" >&2; exit 1; }
	$(MAKE) TARGET_ARCH=riscv64 CROSS_COMPILE="$(CROSS_COMPILE)" CCEXE=tcc TCC="$(TCC)" clean
	$(MAKE) TARGET_ARCH=riscv64 CROSS_COMPILE="$(CROSS_COMPILE)" CCEXE=tcc TCC="$(TCC)" QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" test-riscv64

test-fd-limit:
	HOSTCC="$(HOSTCC)" tests/fd-limit.sh

test-riscv64-generic-compile: test-fd-limit
	@test "$(TARGET_ARCH)" = riscv64 || { echo "test-riscv64-generic-compile requires TARGET_ARCH=riscv64" >&2; exit 1; }
	GENERIC_CC="$(CROSS_COMPILE)gcc" GENERIC_LD="$(CROSS_COMPILE)ld" \
		AS="$(CROSS_COMPILE)as" NM="$(CROSS_COMPILE)nm" \
		tests/riscv64-generic-link.sh
	AS="$(CROSS_COMPILE)as" NM="$(CROSS_COMPILE)nm" tests/riscv64-generic-trap.sh
	HOSTCC="$(HOSTCC)" tests/riscv64-generic-trap-policy.sh
	HOSTCC="$(HOSTCC)" QEMU="$(QEMU)" tests/riscv64-fdt.sh
	HOSTCC="$(HOSTCC)" tests/riscv64-page-fault-policy.sh
	HOSTCC="$(HOSTCC)" tests/riscv64-signal-frame.sh
	HOSTCC="$(HOSTCC)" tests/riscv64-signal-policy.sh
	GENERIC_CC="$(CROSS_COMPILE)gcc" tests/riscv64-signal-uapi.sh
	AS="$(CROSS_COMPILE)as" LD="$(CROSS_COMPILE)ld" QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" tests/riscv64-generic-trap-runtime.sh
	HOSTCC="$(HOSTCC)" tests/riscv64-elf64-plan.sh
	HOSTCC="$(HOSTCC)" tests/riscv64-syscall-translation.sh

riscv64-generic-image:
	@test "$(TARGET_ARCH)" = riscv64 || { echo "riscv64-generic-image requires TARGET_ARCH=riscv64" >&2; exit 1; }
	GENERIC_CC="$(if $(GENERIC_CC),$(GENERIC_CC),$(CROSS_COMPILE)gcc)" \
		GENERIC_LD="$(CROSS_COMPILE)ld" GENERIC_LDFLAGS="$(GENERIC_LDFLAGS)" \
		GENERIC_RUNTIME="$(GENERIC_RUNTIME)" \
		GENERIC_RETAINED_STUBS="$(GENERIC_RETAINED_STUBS)" \
		GENERIC_WORKDIR="$(GENERIC_WORKDIR)" \
		GENERIC_MARCH="$(if $(GENERIC_MARCH),$(GENERIC_MARCH),$(RISCV_MARCH))" \
		AS="$(CROSS_COMPILE)as" NM="$(CROSS_COMPILE)nm" \
		READELF="$(CROSS_COMPILE)readelf" GENERIC_IMAGE=fiwix-generic \
		tests/riscv64-generic-image.sh

riscv64-generic-image-tcc:
	@test "$(TARGET_ARCH)" = riscv64 || { echo "riscv64-generic-image-tcc requires TARGET_ARCH=riscv64" >&2; exit 1; }
	@test -f "$(TCC_LIBTCC1)" || { echo "riscv64-generic-image-tcc requires TCC_LIBTCC1=/path/to/libtcc1.a" >&2; exit 1; }
	$(MAKE) TARGET_ARCH=riscv64 CROSS_COMPILE="$(CROSS_COMPILE)" \
		GENERIC_CC="$(TCC)" GENERIC_RUNTIME="$(TCC_LIBTCC1)" \
		GENERIC_LDFLAGS="--no-warn-mismatch" \
		GENERIC_RETAINED_STUBS="tests/riscv64-generic-tcc-stubs.expected" \
		riscv64-generic-image

test-riscv64-generic-tcc: riscv64-generic-image-tcc riscv64-generic-disk
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" \
		tests/riscv64-generic-boot-smoke.sh ./fiwix-generic \
		arch/riscv64/fixture/disk.img

riscv64-generic-disk:
	$(MAKE) -C arch/riscv64 fixture/disk.img

test-riscv64-generic-boot: riscv64-generic-image riscv64-generic-disk
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" \
		tests/riscv64-generic-boot-smoke.sh ./fiwix-generic \
		arch/riscv64/fixture/disk.img

riscv64-stage0-init:
	$(MAKE) -C arch/riscv64 fixture/stage0-init.elf

test-riscv64-stage0: riscv64-generic-image riscv64-stage0-init
	@test -n "$(STAGE0_SEED)" || { echo "test-riscv64-stage0 requires STAGE0_SEED=/path/to/hex0-seed" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_SEED="$(STAGE0_SEED)" \
		tests/riscv64-stage0-boot-smoke.sh ./fiwix-generic \
		arch/riscv64/fixture/stage0-init.elf

riscv64-kaem-seed-init:
	$(MAKE) -C arch/riscv64 fixture/kaem-seed-init.elf

riscv64-kaem-complete:
	$(MAKE) -C arch/riscv64 fixture/kaem-complete.elf

riscv64-kaem-linux-complete:
	$(MAKE) -C arch/riscv64 fixture/kaem-linux-complete.elf

riscv64-kaem-manifest1-complete:
	$(MAKE) -C arch/riscv64 fixture/kaem-manifest1-complete.elf

riscv64-kaem-manifest2-complete:
	$(MAKE) -C arch/riscv64 fixture/kaem-manifest2-complete.elf

riscv64-kaem-manifest3-complete:
	$(MAKE) -C arch/riscv64 fixture/kaem-manifest3-complete.elf

riscv64-kaem-manifest4-complete:
	$(MAKE) -C arch/riscv64 fixture/kaem-manifest4-complete.elf

riscv64-kaem-manifest5-complete:
	$(MAKE) -C arch/riscv64 fixture/kaem-manifest5-complete.elf

test-riscv64-kaem-seed: riscv64-generic-image riscv64-kaem-seed-init
	@test -n "$(STAGE0_DIR)" || { echo "test-riscv64-kaem-seed requires STAGE0_DIR=/path/to/stage0-posix" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		tests/riscv64-kaem-seed-boot-smoke.sh ./fiwix-generic \
		arch/riscv64/fixture/kaem-seed-init.elf

test-riscv64-kaem-phase2: TIMEOUT=60
test-riscv64-kaem-phase2: riscv64-generic-image riscv64-kaem-seed-init
	@test -n "$(STAGE0_DIR)" || { echo "test-riscv64-kaem-phase2 requires STAGE0_DIR=/path/to/stage0-posix" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		KAEM_STAGE=phase2 tests/riscv64-kaem-seed-boot-smoke.sh \
		./fiwix-generic arch/riscv64/fixture/kaem-seed-init.elf

test-riscv64-kaem-phase3: TIMEOUT=60
test-riscv64-kaem-phase3: riscv64-generic-image riscv64-kaem-seed-init
	@test -n "$(STAGE0_DIR)" || { echo "test-riscv64-kaem-phase3 requires STAGE0_DIR=/path/to/stage0-posix" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		KAEM_STAGE=phase3 tests/riscv64-kaem-seed-boot-smoke.sh \
		./fiwix-generic arch/riscv64/fixture/kaem-seed-init.elf

test-riscv64-kaem-phase4: TIMEOUT=90
test-riscv64-kaem-phase4: riscv64-generic-image riscv64-kaem-seed-init
	@test -n "$(STAGE0_DIR)" || { echo "test-riscv64-kaem-phase4 requires STAGE0_DIR=/path/to/stage0-posix" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		KAEM_STAGE=phase4 tests/riscv64-kaem-seed-boot-smoke.sh \
		./fiwix-generic arch/riscv64/fixture/kaem-seed-init.elf

test-riscv64-kaem-mini: TIMEOUT=3600
test-riscv64-kaem-mini: riscv64-generic-image riscv64-kaem-seed-init riscv64-kaem-complete
	@test -n "$(STAGE0_DIR)" || { echo "test-riscv64-kaem-mini requires STAGE0_DIR=/path/to/stage0-posix" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		KAEM_STAGE=mini tests/riscv64-kaem-seed-boot-smoke.sh \
		./fiwix-generic arch/riscv64/fixture/kaem-seed-init.elf

test-riscv64-kaem-manifest1: TIMEOUT=3600
test-riscv64-kaem-manifest1: riscv64-generic-image riscv64-kaem-seed-init riscv64-kaem-manifest1-complete
	@test -n "$(STAGE0_DIR)" || { echo "test-riscv64-kaem-manifest1 requires STAGE0_DIR=/path/to/stage0-posix" >&2; exit 1; }
	@test -n "$(LIVE_BOOTSTRAP_DIR)" || { echo "test-riscv64-kaem-manifest1 requires LIVE_BOOTSTRAP_DIR=/path/to/live-bootstrap" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		LIVE_BOOTSTRAP_DIR="$(LIVE_BOOTSTRAP_DIR)" \
		KAEM_STAGE=manifest1 tests/riscv64-kaem-seed-boot-smoke.sh \
		./fiwix-generic arch/riscv64/fixture/kaem-seed-init.elf \
		arch/riscv64/fixture/kaem-manifest1-complete.elf

test-riscv64-kaem-manifest2: TIMEOUT=3600
test-riscv64-kaem-manifest2: riscv64-generic-image riscv64-kaem-seed-init riscv64-kaem-manifest2-complete
	@test -n "$(STAGE0_DIR)" || { echo "test-riscv64-kaem-manifest2 requires STAGE0_DIR=/path/to/stage0-posix" >&2; exit 1; }
	@test -n "$(LIVE_BOOTSTRAP_DIR)" || { echo "test-riscv64-kaem-manifest2 requires LIVE_BOOTSTRAP_DIR=/path/to/live-bootstrap" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		LIVE_BOOTSTRAP_DIR="$(LIVE_BOOTSTRAP_DIR)" \
		KAEM_STAGE=manifest2 tests/riscv64-kaem-seed-boot-smoke.sh \
		./fiwix-generic arch/riscv64/fixture/kaem-seed-init.elf \
		arch/riscv64/fixture/kaem-manifest2-complete.elf

test-riscv64-kaem-manifest3: TIMEOUT=345600
test-riscv64-kaem-manifest3: riscv64-generic-image riscv64-kaem-seed-init riscv64-kaem-manifest3-complete
	@test -n "$(STAGE0_DIR)" || { echo "test-riscv64-kaem-manifest3 requires STAGE0_DIR=/path/to/stage0-posix" >&2; exit 1; }
	@test -n "$(LIVE_BOOTSTRAP_DIR)" || { echo "test-riscv64-kaem-manifest3 requires LIVE_BOOTSTRAP_DIR=/path/to/live-bootstrap" >&2; exit 1; }
	@test -n "$(LIVE_BOOTSTRAP_DISTFILES)" || { echo "test-riscv64-kaem-manifest3 requires LIVE_BOOTSTRAP_DISTFILES=/path/to/distfiles" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		LIVE_BOOTSTRAP_DIR="$(LIVE_BOOTSTRAP_DIR)" \
		LIVE_BOOTSTRAP_DISTFILES="$(LIVE_BOOTSTRAP_DISTFILES)" \
		KAEM_STAGE=manifest3 tests/riscv64-kaem-seed-boot-smoke.sh \
		./fiwix-generic arch/riscv64/fixture/kaem-seed-init.elf \
		arch/riscv64/fixture/kaem-manifest3-complete.elf

test-riscv64-kaem-manifest4: TIMEOUT=345600
test-riscv64-kaem-manifest4: riscv64-generic-image riscv64-kaem-seed-init riscv64-kaem-manifest4-complete
	@test -n "$(STAGE0_DIR)" || { echo "test-riscv64-kaem-manifest4 requires STAGE0_DIR=/path/to/stage0-posix" >&2; exit 1; }
	@test -n "$(LIVE_BOOTSTRAP_DIR)" || { echo "test-riscv64-kaem-manifest4 requires LIVE_BOOTSTRAP_DIR=/path/to/live-bootstrap" >&2; exit 1; }
	@test -n "$(LIVE_BOOTSTRAP_DISTFILES)" || { echo "test-riscv64-kaem-manifest4 requires LIVE_BOOTSTRAP_DISTFILES=/path/to/distfiles" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		LIVE_BOOTSTRAP_DIR="$(LIVE_BOOTSTRAP_DIR)" \
		LIVE_BOOTSTRAP_DISTFILES="$(LIVE_BOOTSTRAP_DISTFILES)" \
		KAEM_STAGE=manifest4 tests/riscv64-kaem-seed-boot-smoke.sh \
		./fiwix-generic arch/riscv64/fixture/kaem-seed-init.elf \
		arch/riscv64/fixture/kaem-manifest4-complete.elf

test-riscv64-kaem-manifest5: TIMEOUT=345600
test-riscv64-kaem-manifest5: riscv64-generic-image riscv64-kaem-seed-init riscv64-kaem-manifest5-complete
	@test -n "$(STAGE0_DIR)" || { echo "test-riscv64-kaem-manifest5 requires STAGE0_DIR=/path/to/stage0-posix" >&2; exit 1; }
	@test -n "$(LIVE_BOOTSTRAP_DIR)" || { echo "test-riscv64-kaem-manifest5 requires LIVE_BOOTSTRAP_DIR=/path/to/live-bootstrap" >&2; exit 1; }
	@test -n "$(LIVE_BOOTSTRAP_DISTFILES)" || { echo "test-riscv64-kaem-manifest5 requires LIVE_BOOTSTRAP_DISTFILES=/path/to/distfiles" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		LIVE_BOOTSTRAP_DIR="$(LIVE_BOOTSTRAP_DIR)" \
		LIVE_BOOTSTRAP_DISTFILES="$(LIVE_BOOTSTRAP_DISTFILES)" \
		KAEM_STAGE=manifest5 tests/riscv64-kaem-seed-boot-smoke.sh \
		./fiwix-generic arch/riscv64/fixture/kaem-seed-init.elf \
		arch/riscv64/fixture/kaem-manifest5-complete.elf

test-riscv64-kaem-stage0-linux: TIMEOUT=3600
test-riscv64-kaem-stage0-linux: riscv64-generic-image riscv64-kaem-seed-init riscv64-kaem-linux-complete riscv64-linux-kaem-init riscv64-linux-stage0-complete
	@test -n "$(STAGE0_DIR)" || { echo "test-riscv64-kaem-stage0-linux requires STAGE0_DIR=/path/to/stage0-posix" >&2; exit 1; }
	@test -n "$(LINUX_IMAGE)" || { echo "test-riscv64-kaem-stage0-linux requires LINUX_IMAGE=/path/to/Image" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		LINUX_IMAGE="$(LINUX_IMAGE)" \
		LINUX_INIT=arch/riscv64/fixture/linux-kaem-init.elf \
		KAEM_STAGE=stage0-linux tests/riscv64-kaem-seed-boot-smoke.sh \
		./fiwix-generic arch/riscv64/fixture/kaem-seed-init.elf \
		arch/riscv64/fixture/linux-stage0-complete.elf

test-riscv64-kaem-linux: TIMEOUT=3600
test-riscv64-kaem-linux: riscv64-generic-image riscv64-kaem-seed-init riscv64-kaem-linux-complete riscv64-linux-root-init
	@test -n "$(STAGE0_DIR)" || { echo "test-riscv64-kaem-linux requires STAGE0_DIR=/path/to/stage0-posix" >&2; exit 1; }
	@test -n "$(LINUX_IMAGE)" || { echo "test-riscv64-kaem-linux requires LINUX_IMAGE=/path/to/Image" >&2; exit 1; }
	QEMU="$(QEMU)" TIMEOUT="$(TIMEOUT)" STAGE0_DIR="$(STAGE0_DIR)" \
		LINUX_IMAGE="$(LINUX_IMAGE)" \
		LINUX_INIT=arch/riscv64/fixture/linux-root-init.elf \
		KAEM_STAGE=linux tests/riscv64-kaem-seed-boot-smoke.sh \
		./fiwix-generic arch/riscv64/fixture/kaem-seed-init.elf \
		arch/riscv64/fixture/kaem-linux-complete.elf

.PHONY: all clean test-arm test-arm-serial-line test-arm-generic-compile arm-generic-image arm-generic-image-tcc arm-generic-disk arm-linux-root-init arm-linux-root-disk test-arm-linux-root arm-linux-tcc-init test-arm-linux-tcc arm-linux-tcc-seed-init test-arm-linux-tcc-seed arm-m2-pivot-init test-arm-m2-pivot arm-mes-init test-arm-mes arm-mescc-init test-arm-mescc arm-tcc-seed-init test-arm-tcc-seed arm-tcc-init test-arm-tcc arm-tcc-selfhost-init test-arm-tcc-selfhost test-arm-generic-tcc test-arm-generic-boot test-riscv64 test-riscv64-large-image test-riscv64-linux riscv64-linux-root-init riscv64-linux-kaem-init riscv64-linux-stage0-complete riscv64-linux-root-disk test-riscv64-linux-root test-riscv64-tcc test-fd-limit test-riscv64-generic-compile riscv64-generic-image riscv64-generic-image-tcc test-riscv64-generic-tcc riscv64-generic-disk test-riscv64-generic-boot riscv64-stage0-init test-riscv64-stage0 riscv64-kaem-seed-init riscv64-kaem-complete riscv64-kaem-linux-complete riscv64-kaem-manifest1-complete riscv64-kaem-manifest2-complete riscv64-kaem-manifest3-complete riscv64-kaem-manifest4-complete riscv64-kaem-manifest5-complete test-riscv64-kaem-seed test-riscv64-kaem-phase2 test-riscv64-kaem-phase3 test-riscv64-kaem-phase4 test-riscv64-kaem-mini test-riscv64-kaem-manifest1 test-riscv64-kaem-manifest2 test-riscv64-kaem-manifest3 test-riscv64-kaem-manifest4 test-riscv64-kaem-manifest5 test-riscv64-kaem-stage0-linux test-riscv64-kaem-linux
