LWIPDIR = .
include Filelists.mk

.c.o:
	$(CC) $(CFLAGS) -I$(FIWIX_INCLUDE) -Iinclude -c -o $@ $<

OBJS = $(LWIPNOAPPSFILES:.c=.o) sys_arch.o

liblwip.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

all: liblwip.a

clean:
	rm -f $(OBJS) liblwip.a

