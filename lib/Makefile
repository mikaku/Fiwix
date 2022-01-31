# fiwix/lib/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = ctype.o strings.o printk.o

all:	$(OBJS)

clean:
	rm -f *.o

