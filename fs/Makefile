# fiwix/fs/Makefile
#
# Copyright 2018, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

FSDIRS = minix ext2 pipefs iso9660 procfs
FILESYSTEMS = minix/minix.o ext2/ext2.o pipefs/pipefs.o iso9660/iso9660.o \
	procfs/procfs.o
OBJS = filesystems.o devices.o buffer.o fd.o locks.o super.o inode.o \
	namei.o elf.o

fs:	$(OBJS)
	@for n in $(FSDIRS) ; do (cd $$n ; $(MAKE)) ; done
	$(LD) $(LDFLAGS) -r $(FILESYSTEMS) $(OBJS) -o fs.o

clean:
	@for n in $(FSDIRS) ; do (cd $$n ; $(MAKE) clean) ; done
	rm -f *.o

