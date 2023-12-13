# fiwix/fs/sockfs/Makefile
#
# Copyright 2023, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = super.o socket.o

all:	$(OBJS)

clean:
	rm -f *.o

