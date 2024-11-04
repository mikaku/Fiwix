# fiwix/fs/procfs/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = super.o inode.o namei.o dir.o file.o symlink.o tree.o data.o

all:	$(OBJS)

clean:
	rm -f *.o

