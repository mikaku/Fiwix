# fiwix/mm/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = bios_map.o buddy_low.o memory.o page.o alloc.o fault.o mmap.o swapper.o

all:	$(OBJS)

clean:
	rm -f *.o

