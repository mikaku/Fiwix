# fiwix/drivers/net/Makefile
#
# Copyright 2026, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = netdevice.o loopback.o

all:	$(OBJS)

clean:
	rm -f *.o

