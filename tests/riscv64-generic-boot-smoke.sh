#!/bin/sh

set -eu

QEMU=${QEMU:-qemu-system-riscv64}
TIMEOUT=${TIMEOUT:-10}
kernel=${1:-./fiwix-generic}
disk=${2:-arch/riscv64/fixture/disk.img}
temporary=$(mktemp)
modern=$(mktemp)
legacy_disk=$(mktemp)
modern_disk=$(mktemp)
qemu_pid=

cleanup()
{
	if test -n "$qemu_pid"; then
		kill "$qemu_pid" 2>/dev/null || true
		wait "$qemu_pid" 2>/dev/null || true
	fi
	rm -f "$temporary" "$modern" "$legacy_disk" "$modern_disk"
}

trap cleanup EXIT
trap 'exit 1' HUP INT TERM

cp "$disk" "$legacy_disk"
cp "$disk" "$modern_disk"

tests/riscv64-ext2-check.sh "$disk"

markers_present()
{
	grep -q 'Fiwix riscv64 Goldfish RTC gate passed' "$output" &&
		grep -q 'Fiwix riscv64 generic PID 1 construction passed' \
		"$output" &&
		grep -q 'Fiwix riscv64 PID 1 userspace passed' "$output"
}

run_qemu()
{
	output=$1
	run_disk=$2
	shift 2
	"$QEMU" -machine virt -m 256M -smp 1 \
		-nographic -bios none -kernel "$kernel" -no-reboot \
		-drive file="$run_disk",format=raw,if=none,id=drive0 \
		-device virtio-blk-device,drive=drive0 "$@" > "$output" 2>&1 &
	qemu_pid=$!
	elapsed=0
	while kill -0 "$qemu_pid" 2>/dev/null; do
		if markers_present; then
			kill "$qemu_pid" 2>/dev/null || true
			wait "$qemu_pid" 2>/dev/null || true
			qemu_pid=
			return
		fi
		if test "$elapsed" -ge "$TIMEOUT"; then
			kill "$qemu_pid" 2>/dev/null || true
			wait "$qemu_pid" 2>/dev/null || true
			qemu_pid=
			cat "$output" >&2
			exit 1
		fi
		sleep 1
		elapsed=$((elapsed + 1))
	done
	if wait "$qemu_pid"; then
		status=0
	else
		status=$?
	fi
	qemu_pid=
	if markers_present; then
		return
	fi
	cat "$output" >&2
	test "$status" -eq 0 || exit "$status"
	exit 1
}

run_qemu "$temporary" "$legacy_disk"
run_qemu "$modern" "$modern_disk" -global virtio-mmio.force-legacy=false

echo "Fiwix riscv64 generic boot smoke passed"
