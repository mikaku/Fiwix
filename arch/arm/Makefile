# fiwix/arch/arm/Makefile
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

.DEFAULT_GOAL := all
HOSTCC ?= cc

.S.o:
	$(AS) $(ASFLAGS) -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = boot.o context.o elf32.o ops.o traps.o main.o vm.o

all: $(OBJS)

fixture/make-arm-disk: ../../tests/make-arm-disk.c
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

fixture/disk.img: fixture/make-arm-disk fixture/generic-init.elf FORCE
	fixture/make-arm-disk $@ fixture/generic-init.elf

FORCE:

clean:
	rm -f *.o fixture/*.o fixture/*.elf fixture/*.img \
		fixture/make-arm-disk
	rmdir fixture 2>/dev/null || true

.PHONY: all clean FORCE
