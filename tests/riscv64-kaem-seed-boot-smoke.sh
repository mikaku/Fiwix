#!/bin/sh

set -eu

QEMU=${QEMU:-qemu-system-riscv64}
TIMEOUT=${TIMEOUT:-20}
MKE2FS=${MKE2FS:-mke2fs}
DEBUGFS=${DEBUGFS:-debugfs}
SHA256SUM=${SHA256SUM:-sha256sum}
TAR=${TAR:-tar}
STAGE0_DIR=${STAGE0_DIR:?set STAGE0_DIR to the stage0-posix checkout}
KAEM_WORKDIR=${KAEM_WORKDIR:-}
KAEM_STAGE=${KAEM_STAGE:-seed}
KAEM_TRANSPORT=${KAEM_TRANSPORT:-all}
kernel=${1:-./fiwix-generic}
launcher=${2:-arch/riscv64/fixture/kaem-seed-init.elf}
root=$(cd "$(dirname "$0")/.." && pwd)
stage0=$(cd "$STAGE0_DIR" && pwd)

case $KAEM_STAGE in
	seed|phase2|mini) ;;
	*) echo "unsupported KAEM_STAGE: $KAEM_STAGE" >&2; exit 1 ;;
esac
case $KAEM_TRANSPORT in
	all|legacy|modern) ;;
	*) echo "unsupported KAEM_TRANSPORT: $KAEM_TRANSPORT" >&2; exit 1 ;;
esac

if test -n "$KAEM_WORKDIR"; then
	test "$KAEM_WORKDIR" != / || {
		echo "refusing unsafe KAEM_WORKDIR=/" >&2
		exit 1
	}
	work=$KAEM_WORKDIR
	rm -rf "$work"
	mkdir -p "$work"
else
	work=$(mktemp -d)
	trap 'rm -rf "$work"' EXIT HUP INT TERM
fi

hex0_seed=$stage0/bootstrap-seeds/POSIX/riscv64/hex0-seed
kaem_seed=$stage0/bootstrap-seeds/POSIX/riscv64/kaem-optional-seed
hex0_input=$stage0/riscv64/hex0_riscv64.hex0
kaem_input=$stage0/riscv64/kaem-minimal.hex0
seed_script=$stage0/riscv64/mescc-tools-seed-kaem.kaem

for input in "$hex0_seed" "$kaem_seed" "$hex0_input" "$kaem_input" \
	"$seed_script"; do
	test -f "$input" || {
		echo "missing stage0-posix input: $input" >&2
		exit 1
	}
done

command -v "$SHA256SUM" >/dev/null
command -v "$TAR" >/dev/null
check_hash()
{
	file=$1
	expected=$2
	actual=$("$SHA256SUM" "$file")
	actual=${actual%% *}
	test "$actual" = "$expected" || {
		echo "unexpected checksum for $file" >&2
		exit 1
	}
}
check_hash "$hex0_seed" \
	1b50ceef632b83b79aef0cf91d60bc0cb242a3b2bfba22cb5115d80112b50ac9
check_hash "$kaem_seed" \
	12c0a7d01f2e369598ffdfc6e0881d62d20621d5f0d9fa9580ee511d72300650

if ! command -v "$MKE2FS" >/dev/null 2>&1 && test -x /sbin/mke2fs; then
	MKE2FS=/sbin/mke2fs
fi
if ! command -v "$DEBUGFS" >/dev/null 2>&1 && test -x /sbin/debugfs; then
	DEBUGFS=/sbin/debugfs
fi
command -v "$MKE2FS" >/dev/null
command -v "$DEBUGFS" >/dev/null

rootfs=$work/rootfs
disk=$work/kaem-seed.img
legacy_disk=$work/kaem-seed-legacy.img
modern_disk=$work/kaem-seed-modern.img
if test "$KAEM_STAGE" != seed; then
	expected_stage0_commit=643598041bf7639883874fe2cdc9d9693c9b03d5
	actual_stage0_commit=$(git -C "$stage0" rev-parse HEAD)
	test "$actual_stage0_commit" = "$expected_stage0_commit" || {
		echo "stage0-posix checkout is not at $expected_stage0_commit" >&2
		exit 1
	}
	mkdir -p "$rootfs"
	git -C "$stage0" archive --format=tar HEAD | "$TAR" -xf - -C "$rootfs"
	git -C "$stage0" submodule status --recursive > "$work/submodules"
	while read revision path description; do
		case $revision in
			[-+U]*)
				echo "stage0-posix submodule is not pinned: $path" >&2
				exit 1
				;;
		esac
		git -C "$stage0/$path" archive --format=tar \
			--prefix="$path/" HEAD > "$work/submodule.tar"
		"$TAR" -xf "$work/submodule.tar" -C "$rootfs"
	done < "$work/submodules"
	if test "$KAEM_STAGE" = phase2; then
		install -m 644 "$root/tests/fixtures/riscv64-kaem-phase2.kaem" \
			"$rootfs/riscv64/mescc-tools-seed-kaem.kaem"
		install -m 644 \
			"$root/tests/fixtures/riscv64-kaem-phase2-body.kaem" \
			"$rootfs/riscv64/mescc-tools-phase2-kaem.kaem"
	else
		install -m 644 "$root/tests/fixtures/riscv64-kaem-mini.kaem" \
			"$rootfs/riscv64/mescc-tools-seed-kaem.kaem"
	fi
	disk_blocks=131072
else
	mkdir -p "$rootfs/bootstrap-seeds/POSIX/riscv64" \
		"$rootfs/riscv64/artifact"
	install -m 755 "$hex0_seed" \
		"$rootfs/bootstrap-seeds/POSIX/riscv64/hex0-seed"
	install -m 755 "$kaem_seed" \
		"$rootfs/bootstrap-seeds/POSIX/riscv64/kaem-optional-seed"
	install -m 644 "$hex0_input" "$rootfs/riscv64/hex0_riscv64.hex0"
	install -m 644 "$kaem_input" "$rootfs/riscv64/kaem-minimal.hex0"
	install -m 644 "$seed_script" \
		"$rootfs/riscv64/mescc-tools-seed-kaem.kaem"
	disk_blocks=8192
fi
mkdir -p "$rootfs/dev" "$rootfs/sbin"
install -m 755 "$launcher" "$rootfs/sbin/init"
printf '%s\n' 'Fiwix riscv64 ext2 file gate passed' > "$rootfs/bootstrap"

"$MKE2FS" -q -F -t ext2 -b 1024 -I 128 -r 0 -O none \
	-d "$rootfs" "$disk" "$disk_blocks"
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
	hex0_actual=$work/$name.hex0
	kaem_actual=$work/$name.kaem-0
	mini_m1_actual=$work/$name.M1
	mini_hex2_actual=$work/$name.hex2
	mini_kaem_actual=$work/$name.kaem
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
	if ! grep -q 'Fiwix riscv64 kaem seed launcher entered' "$serial" ||
		grep -q 'kaem seed launcher exec failed\|Subprocess error\|fatal\|PANIC' \
		"$serial"; then
		cat "$serial" >&2
		exit 1
	fi
	"$DEBUGFS" -R 'cat /riscv64/artifact/hex0' "$run_disk" \
		> "$hex0_actual" 2>/dev/null
	"$DEBUGFS" -R 'cat /riscv64/artifact/kaem-0' "$run_disk" \
		> "$kaem_actual" 2>/dev/null
	if ! cmp -s "$hex0_seed" "$hex0_actual" ||
		! cmp -s "$kaem_seed" "$kaem_actual"; then
		cat "$serial" >&2
		"$SHA256SUM" "$hex0_seed" "$hex0_actual" "$kaem_seed" \
			"$kaem_actual" >&2
		exit 1
	fi
	if test "$KAEM_STAGE" = mini; then
		"$DEBUGFS" -R 'cat /riscv64/bin/M1' "$run_disk" \
			> "$mini_m1_actual" 2>/dev/null
		"$DEBUGFS" -R 'cat /riscv64/bin/hex2' "$run_disk" \
			> "$mini_hex2_actual" 2>/dev/null
		"$DEBUGFS" -R 'cat /riscv64/bin/kaem' "$run_disk" \
			> "$mini_kaem_actual" 2>/dev/null
		check_hash "$mini_m1_actual" \
			4a917f903f07f0c45bfcf440c4c1fa8401f896c9e4c4086cc2295f01f914e74e
		check_hash "$mini_hex2_actual" \
			96ce368b0ec942470c08a8cbe197f843781ca2d1f4a7a64d3908d5f62c3d0d07
		check_hash "$mini_kaem_actual" \
			0c2fdf2057b404707c6c589280c5bf99b33a62b6649bfd0cedbb699a3d0d7c00
	elif test "$KAEM_STAGE" = phase2; then
		phase2_hex1_actual=$work/$name.hex1
		phase2_hex2_actual=$work/$name.hex2-0
		"$DEBUGFS" -R 'cat /riscv64/artifact/hex1' "$run_disk" \
			> "$phase2_hex1_actual" 2>/dev/null
		"$DEBUGFS" -R 'cat /riscv64/artifact/hex2-0' "$run_disk" \
			> "$phase2_hex2_actual" 2>/dev/null
		check_hash "$phase2_hex1_actual" \
			2c0037d9455f282d5612c1cf280b6a681a33ee1fd633375276e4a816101a3574
		check_hash "$phase2_hex2_actual" \
			93b9073e0fbdd03da34bc3d6b47302157b11a18a6753161b2e77384dc4347459
	fi
	"$root/tests/riscv64-ext2-check.sh" "$run_disk"
}

if test "$KAEM_TRANSPORT" = all || test "$KAEM_TRANSPORT" = legacy; then
	run_qemu legacy "$legacy_disk"
fi
if test "$KAEM_TRANSPORT" = all || test "$KAEM_TRANSPORT" = modern; then
	run_qemu modern "$modern_disk" -global virtio-mmio.force-legacy=false
fi

echo "Fiwix riscv64 kaem $KAEM_STAGE process-tree boot passed"
