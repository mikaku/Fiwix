# fiwix/fs/iso9660/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = inode.o super.o namei.o dir.o file.o rrip.o symlink.o

all:	$(OBJS)

clean:
	rm -f *.o

