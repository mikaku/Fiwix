# fiwix/drivers/video/Makefile
#
# Copyright 2021, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = video_init.o vgacon.o fbcon.o fonts.o

video:	$(OBJS)
	$(LD) $(LDFLAGS) -r $(OBJS) -o video.o

clean:
	rm -f *.o

