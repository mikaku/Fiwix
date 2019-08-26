# fiwix/fs/ext2/Makefile
#
# Copyright 2018, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = inode.o super.o namei.o symlink.o dir.o file.o bitmaps.o

ext2:	$(OBJS)
	$(LD) $(LDFLAGS) -r $(OBJS) -o ext2.o

clean:
	rm -f *.o

