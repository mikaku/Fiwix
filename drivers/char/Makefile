# fiwix/drivers/char/Makefile
#
# Copyright 2018, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = console.o tty.o tty_queue.o vt.o defkeymap.o keyboard.o memdev.o serial.o lp.o

char:	$(OBJS)
	$(LD) $(LDFLAGS) -r $(OBJS) -o char.o

clean:
	rm -f *.o

