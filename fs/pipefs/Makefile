# fiwix/fs/pipefs/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = super.o fifo.o pipe.o

all:	$(OBJS)

clean:
	rm -f *.o

