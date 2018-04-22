# fiwix/kernel/syscalls/Makefile
#
# Copyright 2018, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

SRC = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRC))

lib:	$(OBJS)
	$(LD) $(LDFLAGS) -r $(OBJS) -o syscalls.o

clean:
	rm -f *.o

