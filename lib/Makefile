# fiwix/lib/Makefile
#
# Copyright 2018, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = ctype.o strings.o printk.o

lib:	$(OBJS)
	$(LD) $(LDFLAGS) -r $(OBJS) -o lib.o

clean:
	rm -f *.o

