# fiwix/fs/iso9660/Makefile
#
# Copyright 2018, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = inode.o super.o namei.o dir.o file.o rrip.o symlink.o

iso9660:$(OBJS)
	$(LD) $(LDFLAGS) -r $(OBJS) -o iso9660.o

clean:
	rm -f *.o

