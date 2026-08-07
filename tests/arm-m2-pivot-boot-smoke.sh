#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

QEMU=${QEMU:-qemu-system-arm}
TIMEOUT=${TIMEOUT:-300}
MKE2FS=${MKE2FS:-}
DEBUGFS=${DEBUGFS:-}
SHA256SUM=${SHA256SUM:-sha256sum}
STAGE0_DIR=${STAGE0_DIR:?set STAGE0_DIR to the completed ARM pivot}
ARMV7_M2=${ARMV7_M2:-"$STAGE0_DIR/M2-Planet-armv7l"}
KERNEL=${1:-./fiwix-arm-generic.bin}
INIT=${2:-arch/arm/fixture/m2-pivot-init.elf}
EXPECTED_SHA256=9c3a8e2878c673b074a51157704fd84c8f92f96b0506c93a390e469b9f8cc543
EXPECTED_BYTES=376490
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
stage0=$(CDPATH='' cd -- "$STAGE0_DIR" && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

find_tool()
{
	name=$1
	explicit=$2
	if test -n "$explicit"; then
		printf '%s\n' "$explicit"
		return
	fi
	if command -v "$name" >/dev/null 2>&1; then
		command -v "$name"
		return
	fi
	for candidate in "/usr/sbin/$name" "/sbin/$name"; do
		if test -x "$candidate"; then
			printf '%s\n' "$candidate"
			return
		fi
	done
	return 1
}

MKE2FS=$(find_tool mke2fs "$MKE2FS")
DEBUGFS=$(find_tool debugfs "$DEBUGFS")
command -v "$QEMU" >/dev/null
command -v "$SHA256SUM" >/dev/null
test -f "$KERNEL"
test -f "$INIT"
test -f "$ARMV7_M2"

check_hash()
{
	file=$1
	expected=$2
	actual=$("$SHA256SUM" "$file")
	actual=${actual%% *}
	test "$actual" = "$expected" || {
		printf 'unexpected checksum for %s: %s\n' "$file" "$actual" >&2
		exit 1
	}
}

check_hash "$ARMV7_M2" \
	a5b4d5e77906b18079203061f06fabb21ec06e5d6a5bfe8d363dc1b395ddf797

rootfs=$temporary/rootfs
disk=$temporary/arm-m2-pivot.img
mkdir -p "$rootfs/dev" "$rootfs/sbin" \
	"$rootfs/M2libc/sys" "$rootfs/M2libc/armv7l/linux"
install -m 755 "$INIT" "$rootfs/sbin/init"
install -m 755 "$ARMV7_M2" "$rootfs/M2-Planet-armv7l"
install -m 644 "$stage0/M2libc/sys/types.h" "$rootfs/M2libc/sys/types.h"
install -m 644 "$stage0/M2libc/stddef.h" "$rootfs/M2libc/stddef.h"
install -m 644 "$stage0/M2libc/armv7l/linux/unistd.c" \
	"$rootfs/M2libc/armv7l/linux/unistd.c"
install -m 644 "$stage0/M2libc/armv7l/linux/fcntl.c" \
	"$rootfs/M2libc/armv7l/linux/fcntl.c"
for input in fcntl.c stdarg.h string.c ctype.c stdlib.c stdio.h stdio.c \
	bootstrappable.c; do
	install -m 644 "$stage0/M2libc/$input" "$rootfs/M2libc/$input"
done
install -m 644 "$root/tests/fixtures/arm32-pivot-hello.c" \
	"$rootfs/arm32-pivot-hello.c"
printf '%s\n' 'Fiwix ARM ext2 writable gate init' > "$rootfs/bootstrap"

"$MKE2FS" -q -F -t ext2 -b 1024 -I 128 -r 0 -O none \
	-d "$rootfs" "$disk" 65536
printf '%s\n' 'cd /dev' 'mknod console c 5 1' > "$temporary/debugfs.commands"
"$DEBUGFS" -w -f "$temporary/debugfs.commands" "$disk" >/dev/null 2>&1
printf '%s\n' 'Fiwix ARM virtio sector gate' |
	dd of="$disk" conv=notrunc status=none
"$root/tests/arm-ext2-check.sh" "$disk"

run_pivot()
{
	transport=$1
	shift
	run_disk=$temporary/arm-m2-pivot-$transport.img
	log=$temporary/arm-m2-pivot-$transport.log
	output=$temporary/arm-m2-pivot-$transport.M1
	cp "$disk" "$run_disk"
	if ! timeout "$TIMEOUT" "$QEMU" \
		-M virt,virtualization=on,secure=off -cpu cortex-a15 \
		-m 256M -smp 1 -nographic -kernel "$KERNEL" -no-reboot \
		-drive file="$run_disk",format=raw,if=none,id=drive0 \
		-device virtio-blk-device,drive=drive0 "$@" >"$log" 2>&1; then
		cat "$log" >&2
		exit 1
	fi
	if ! grep -q '^Fiwix ARM M2 pivot process tree entered' "$log" ||
		! grep -q '^Fiwix ARM M2 pivot process tree completed' "$log" ||
		grep -Eiq 'undefined|unhandled|panic|failed|returned' "$log"; then
		cat "$log" >&2
		exit 1
	fi
	"$DEBUGFS" -R 'cat /M2-output.M1' "$run_disk" \
		>"$output" 2>/dev/null
	test "$(wc -c < "$output")" -eq "$EXPECTED_BYTES"
	check_hash "$output" "$EXPECTED_SHA256"
	"$root/tests/arm-ext2-check.sh" "$run_disk"
}

run_pivot legacy
run_pivot modern -global virtio-mmio.force-legacy=false

printf '%s\n' \
	'Fiwix ARM M2 pivot process-tree boot passed'
