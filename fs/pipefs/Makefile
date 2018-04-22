# fiwix/fs/pipefs/Makefile
#
# Copyright 2018, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = super.o fifo.o pipe.o

pipefs:	$(OBJS)
	$(LD) $(LDFLAGS) -r $(OBJS) -o pipefs.o

clean:
	rm -f *.o

