# fiwix/drivers/char/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = console.o tty.o charq.o vt.o ps2.o defkeymap.o keyboard.o psaux.o \
       memdev.o serial.o lp.o fb.o sysrq.o pty.o

all:	$(OBJS)

clean:
	rm -f *.o

