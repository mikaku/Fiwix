# fiwix/fs/devpts/Makefile
#
# Copyright 2025, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = super.o inode.o namei.o dir.o

all:	$(OBJS)

clean:
	rm -f *.o

