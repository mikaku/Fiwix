# fiwix/net/Makefile
#
# Copyright 2023, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = domains.o socket.o unix.o

all:	$(OBJS)

clean:
	rm -f *.o

