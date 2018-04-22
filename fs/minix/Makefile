# fiwix/fs/minix/Makefile
#
# Copyright 2018, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = super.o bitmaps.o inode.o namei.o symlink.o dir.o file.o v1_inode.o v2_inode.o

minix:	$(OBJS)
	$(LD) $(LDFLAGS) -r $(OBJS) -o minix.o

clean:
	rm -f *.o

