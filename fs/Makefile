# fiwix/fs/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

FSDIRS = minix ext2 pipefs iso9660 procfs
OBJS = filesystems.o devices.o buffer.o fd.o locks.o super.o inode.o \
	namei.o elf.o script.o

all:	$(OBJS)
	@for n in $(FSDIRS) ; do (cd $$n ; $(MAKE)) ; done

clean:
	@for n in $(FSDIRS) ; do (cd $$n ; $(MAKE) clean) ; done
	rm -f *.o

