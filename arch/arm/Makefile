# fiwix/arch/arm/Makefile
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

.DEFAULT_GOAL := all
HOSTCC ?= cc
LINUX_IMAGE ?= fixture/linux.zImage

.S.o:
	$(AS) $(ASFLAGS) -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = boot.o context.o elf32.o ops.o traps.o main.o vm.o

all: $(OBJS)

fixture/make-arm-disk: ../../tests/make-arm-disk.c
	mkdir -p fixture
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -o $@ $<

fixture/make-arm-linux-disk: ../../tests/make-arm-linux-disk.c
	mkdir -p fixture
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -o $@ $<

fixture/generic-init.o: generic-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/generic-init.elf: fixture/generic-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/m2-pivot-init.o: m2-pivot-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/m2-pivot-init.elf: fixture/m2-pivot-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/mes-init.o: mes-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/mes-init.elf: fixture/mes-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/mescc-init.o: mescc-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/mescc-init.elf: fixture/mescc-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/tcc-seed-init.o: tcc-seed-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/tcc-seed-init.elf: fixture/tcc-seed-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/tcc-init.o: tcc-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/tcc-init.elf: fixture/tcc-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/tcc-selfhost-init.o: tcc-selfhost-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/tcc-selfhost-init.elf: fixture/tcc-selfhost-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/linux-root-init.o: linux-root-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/linux-root-init.elf: fixture/linux-root-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/linux-tcc-init.o: linux-tcc-init.S tcc-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/linux-tcc-init.elf: fixture/linux-tcc-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/linux-tcc-seed-init.o: linux-tcc-seed-init.S tcc-seed-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/linux-tcc-seed-init.elf: fixture/linux-tcc-seed-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/disk.img: fixture/make-arm-disk fixture/generic-init.elf FORCE
	fixture/make-arm-disk $@ fixture/generic-init.elf

fixture/linux-root-disk.img: fixture/make-arm-linux-disk \
		$(LINUX_IMAGE) fixture/linux-root-init.elf FORCE
	fixture/make-arm-linux-disk $@ $(LINUX_IMAGE) \
		fixture/linux-root-init.elf

FORCE:

clean:
	rm -f *.o fixture/*.o fixture/*.elf fixture/*.img \
		fixture/make-arm-disk fixture/make-arm-linux-disk
	rmdir fixture 2>/dev/null || true

.PHONY: all clean FORCE
