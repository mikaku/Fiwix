# fiwix/drivers/char/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = console.o tty.o tty_queue.o vt.o ps2.o defkeymap.o keyboard.o psaux.o \
       memdev.o serial.o lp.o fb.o sysrq.o

all:	$(OBJS)

clean:
	rm -f *.o

