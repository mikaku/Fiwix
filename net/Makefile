# fiwix/net/Makefile
#
# Copyright 2023, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = domains.o socket.o packet.o core.o unix.o ipv4.o

all:	$(OBJS)

clean:
	rm -f *.o

