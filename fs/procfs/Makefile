# fiwix/fs/procfs/Makefile
#
# Copyright 2018, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = super.o inode.o namei.o dir.o file.o symlink.o tree.o data.o

procfs:	$(OBJS)
	$(LD) $(LDFLAGS) -r $(OBJS) -o procfs.o

clean:
	rm -f *.o

