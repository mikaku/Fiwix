# fiwix/arch/riscv64/Makefile

.DEFAULT_GOAL := all

.S.o:
	$(CC) $(CFLAGS) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = boot.o context.o elf.o main.o user.o user_fixture_blob.o vm.o

user_fixture_blob.o: user_fixture_blob.S fixture/user.elf
	$(CC) $(CFLAGS) -c -o $@ $<

fixture/user.o: user-fixture.S
	mkdir -p fixture
	$(CC) $(CFLAGS) -c -o $@ $<

fixture/user.elf: fixture/user.o user-fixture.ld
	$(LD) $(LDFLAGS) -T user-fixture.ld -o $@ $<

all: $(OBJS)

clean:
	rm -f *.o fixture/*.o fixture/*.elf
	rmdir fixture 2>/dev/null || true
