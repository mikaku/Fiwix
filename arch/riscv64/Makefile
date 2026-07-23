# fiwix/arch/riscv64/Makefile

.DEFAULT_GOAL := all
HOSTCC ?= cc
LINUX_IMAGE ?= fixture/linux.bin

.S.o:
	$(AS) $(ASFLAGS) -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

OBJS = boot.o context.o elf.o ext2_gate.o fdt.o handoff.o init_trampoline.o linux.o main.o ops.o rtc.o user.o \
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

fixture/kaem-complete.o: kaem-complete.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/kaem-complete.elf: fixture/kaem-complete.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/kaem-linux-complete.o: kaem-linux-complete.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/kaem-linux-complete.elf: fixture/kaem-linux-complete.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/kaem-manifest1-complete.o: kaem-manifest1-complete.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/kaem-manifest1-complete.elf: fixture/kaem-manifest1-complete.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/kaem-manifest2-complete.o: kaem-manifest2-complete.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/kaem-manifest2-complete.elf: fixture/kaem-manifest2-complete.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/kaem-manifest3-complete.o: kaem-manifest3-complete.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/kaem-manifest3-complete.elf: fixture/kaem-manifest3-complete.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/kaem-manifest4-complete.o: kaem-manifest4-complete.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/kaem-manifest4-complete.elf: fixture/kaem-manifest4-complete.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/kaem-manifest5-complete.o: kaem-manifest5-complete.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/kaem-manifest5-complete.elf: fixture/kaem-manifest5-complete.o generic-init.ld
	$(LD) $(LDFLAGS) -T generic-init.ld -o $@ $<

fixture/linux-root-init.o: linux-root-init.S
	mkdir -p fixture
	$(AS) $(ASFLAGS) -o $@ $<

fixture/linux-root-init.elf: fixture/linux-root-init.o generic-init.ld
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

fixture/linux-root-disk.img: fixture/make-riscv64-disk $(LINUX_IMAGE) fixture/linux-root-init.elf FORCE
	fixture/make-riscv64-disk $@ $(LINUX_IMAGE) fixture/linux-root-init.elf

all: $(OBJS) fixture/disk.img

FORCE:

clean:
	rm -f *.o fixture/*.o fixture/*.elf fixture/*.bin fixture/*.img \
		fixture/make-riscv64-disk
	rmdir fixture 2>/dev/null || true

.PHONY: all clean FORCE
