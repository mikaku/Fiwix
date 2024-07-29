# fiwix/drivers/block/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = dma.o floppy.o part.o ata.o ata_pci.o ata_hd.o atapi.o atapi_cd.o \
       ramdisk.o blk_queue.o

all:	$(OBJS)

clean:
	rm -f *.o

