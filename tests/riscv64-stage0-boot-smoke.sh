#!/bin/sh

set -eu

QEMU=${QEMU:-qemu-system-riscv64}
TIMEOUT=${TIMEOUT:-12}
MKE2FS=${MKE2FS:-mke2fs}
DEBUGFS=${DEBUGFS:-debugfs}
SHA256SUM=${SHA256SUM:-sha256sum}
STAGE0_SEED=${STAGE0_SEED:?set STAGE0_SEED to the RV64 hex0-seed ELF}
STAGE0_WORKDIR=${STAGE0_WORKDIR:-}
kernel=${1:-./fiwix-generic}
launcher=${2:-arch/riscv64/fixture/stage0-init.elf}
root=$(cd "$(dirname "$0")/.." && pwd)

if test -n "$STAGE0_WORKDIR"; then
	test "$STAGE0_WORKDIR" != / || {
		echo "refusing unsafe STAGE0_WORKDIR=/" >&2
		exit 1
	}
	work=$STAGE0_WORKDIR
	rm -rf "$work"
	mkdir -p "$work"
else
	work=$(mktemp -d)
	trap 'rm -rf "$work"' EXIT HUP INT TERM
fi

test "$(wc -c < "$STAGE0_SEED")" -eq 392 || {
	echo "stage0 seed is not the expected 392-byte RV64 hex0 seed" >&2
	exit 1
}
command -v "$SHA256SUM" >/dev/null
seed_hash=$("$SHA256SUM" "$STAGE0_SEED")
seed_hash=${seed_hash%% *}
test "$seed_hash" = \
	1b50ceef632b83b79aef0cf91d60bc0cb242a3b2bfba22cb5115d80112b50ac9 || {
	echo "stage0 seed checksum does not match the pinned RV64 hex0 seed" >&2
	exit 1
}
if ! command -v "$MKE2FS" >/dev/null 2>&1 && test -x /sbin/mke2fs; then
	MKE2FS=/sbin/mke2fs
fi
if ! command -v "$DEBUGFS" >/dev/null 2>&1 && test -x /sbin/debugfs; then
	DEBUGFS=/sbin/debugfs
fi
command -v "$MKE2FS" >/dev/null
command -v "$DEBUGFS" >/dev/null

rootfs=$work/rootfs
disk=$work/stage0.img
legacy_disk=$work/stage0-legacy.img
modern_disk=$work/stage0-modern.img
mkdir -p "$rootfs/bin" "$rootfs/dev" "$rootfs/sbin"
install -m 755 "$STAGE0_SEED" "$rootfs/bin/hex0-seed"
install -m 755 "$launcher" "$rootfs/sbin/init"
install -m 644 "$root/tests/fixtures/riscv64-stage0-input.hex0" \
	"$rootfs/stage0-input.hex0"
: > "$rootfs/stage0-output"
printf '%s\n' 'Fiwix riscv64 ext2 file gate passed' > "$rootfs/bootstrap"

"$MKE2FS" -q -F -t ext2 -b 1024 -I 128 -r 0 -O none \
	-d "$rootfs" "$disk" 8192
printf '%s\n' 'cd /dev' 'mknod console c 5 1' > "$work/debugfs.commands"
"$DEBUGFS" -w -f "$work/debugfs.commands" "$disk" >/dev/null 2>&1
printf '%s\n' 'Fiwix riscv64 virtio sector gate' | \
	dd of="$disk" conv=notrunc status=none
"$root/tests/riscv64-ext2-check.sh" "$disk"
cp "$disk" "$legacy_disk"
cp "$disk" "$modern_disk"

run_qemu()
{
	name=$1
	run_disk=$2
	shift 2
	serial=$work/$name.serial
	actual=$work/$name.output
	if timeout "$TIMEOUT" "$QEMU" -machine virt -m 256M -smp 1 \
		-nographic -bios none -kernel "$kernel" -no-reboot \
		-drive file="$run_disk",format=raw,if=none,id=drive0 \
		-device virtio-blk-device,drive=drive0 "$@" > "$serial" 2>&1; then
		status=0
	else
		status=$?
	fi
	if test "$status" -ne 0 && test "$status" -ne 124; then
		cat "$serial" >&2
		exit "$status"
	fi
	if ! grep -q 'Fiwix riscv64 stage0 launcher entered' "$serial" ||
		grep -q 'stage0 launcher exec failed\|fatal\|PANIC' "$serial"; then
		cat "$serial" >&2
		exit 1
	fi
	"$DEBUGFS" -R 'cat /stage0-output' "$run_disk" \
		> "$actual" 2>/dev/null
	if ! cmp -s "$root/tests/fixtures/riscv64-stage0-output.expected" \
		"$actual"; then
		cat "$serial" >&2
		diff -u "$root/tests/fixtures/riscv64-stage0-output.expected" \
			"$actual" >&2 || true
		exit 1
	fi
}

run_qemu legacy "$legacy_disk"
run_qemu modern "$modern_disk" -global virtio-mmio.force-legacy=false

echo "Fiwix riscv64 real stage0 hex0 seed boot passed"
