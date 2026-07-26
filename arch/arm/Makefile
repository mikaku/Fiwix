# fiwix/arch/arm/Makefile

.DEFAULT_GOAL := all

.S.o:
	$(AS) $(ASFLAGS) -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = boot.o main.o

all: $(OBJS)

clean:
	rm -f *.o

.PHONY: all clean
