# fiwix/drivers/video/Makefile
#
# Copyright 2021-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = video.o vgacon.o fbcon.o bga.o fonts.o

all:	$(OBJS)

clean:
	rm -f *.o

