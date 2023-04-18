# fiwix/kernel/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = boot.o core386.o main.o init.o gdt.o idt.o kexec.o syscalls.o pic.o \
       pit.o irq.o traps.o cpu.o cmos.o timer.o sched.o sleep.o signal.o \
       process.o multiboot.o

all:	$(OBJS)

clean:
	rm -f *.o

