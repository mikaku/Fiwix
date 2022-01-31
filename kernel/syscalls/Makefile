# fiwix/kernel/syscalls/Makefile
#
# Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
# Distributed under the terms of the Fiwix License.
#

.S.o:
	$(CC) -traditional -I$(INCLUDE) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

SRC = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRC))

all:	$(OBJS)

clean:
	rm -f *.o

