# fiwix/arch/riscv64/Makefile

.S.o:
	$(CC) $(CFLAGS) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = boot.o context.o main.o

all: $(OBJS)

clean:
	rm -f *.o
