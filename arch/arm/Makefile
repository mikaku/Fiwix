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

fixture/disk.img: fixture/make-arm-disk FORCE
	fixture/make-arm-disk $@

FORCE:

clean:
	rm -f *.o fixture/*.img fixture/make-arm-disk
	rmdir fixture 2>/dev/null || true

.PHONY: all clean FORCE
