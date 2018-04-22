# fiwix/mm/Makefile
#
# Copyright 2018, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = bios_map.o memory.o page.o alloc.o fault.o mmap.o swapper.o

mm:	$(OBJS)
	$(LD) $(LDFLAGS) -r $(OBJS) -o mm.o

clean:
	rm -f *.o

