# fiwix/arch/riscv64/Makefile

.DEFAULT_GOAL := all
HOSTCC ?= cc
LINUX_IMAGE ?= fixture/linux.bin

.S.o:
	$(AS) $(ASFLAGS) -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = boot.o context.o elf.o ext2_gate.o handoff.o init_trampoline.o linux.o main.o ops.o user.o \
	user_fixture_blob.o virtio.o vm.o

user_fixture_blob.o: user_fixture_blob.S fixture/user.elf
	$(AS) $(ASFLAGS) -o $@ $<

fixture/user.o: user-fixture.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/user.elf: fixture/user.o user-fixture.ld
	$(LD) $(LDFLAGS) -T user-fixture.ld -o $@ $<

fixture/generic-init.o: generic-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/generic-init.elf: fixture/generic-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/stage0-init.o: stage0-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/stage0-init.elf: fixture/stage0-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/kaem-seed-init.o: kaem-seed-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/kaem-seed-init.elf: fixture/kaem-seed-init.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/linux.o: linux-fixture.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/linux.elf: fixture/linux.o linux-fixture.ld
	$(LD) $(LDFLAGS) -T linux-fixture.ld -o $@ $<

fixture/linux.bin: fixture/linux.elf
	$(CROSS_COMPILE)objcopy -O binary $< $@

fixture/make-riscv64-disk: ../../tests/make-riscv64-disk.c
	mkdir -p fixture
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -o $@ $<

fixture/disk.img: fixture/make-riscv64-disk $(LINUX_IMAGE) fixture/generic-init.elf FORCE
	fixture/make-riscv64-disk $@ $(LINUX_IMAGE) fixture/generic-init.elf

all: $(OBJS) fixture/disk.img

FORCE:

clean:
	rm -f *.o fixture/*.o fixture/*.elf fixture/*.bin fixture/*.img \
		fixture/make-riscv64-disk
	rmdir fixture 2>/dev/null || true

.PHONY: all clean FORCE
