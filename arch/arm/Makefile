# fiwix/arch/arm/Makefile
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

.DEFAULT_GOAL := all

.S.o:
	$(AS) $(ASFLAGS) -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = boot.o context.o elf32.o ops.o traps.o main.o vm.o

all: $(OBJS)

clean:
	rm -f *.o

.PHONY: all clean
