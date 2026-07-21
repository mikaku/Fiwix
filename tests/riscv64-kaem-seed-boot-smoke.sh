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
LINUX_IMAGE=${LINUX_IMAGE:-}
LINUX_INIT=${LINUX_INIT:-}
LINUX_CMDLINE=${LINUX_CMDLINE:-earlycon=uart8250,mmio,0x10000000 console=ttyS0,115200 root=/dev/vda ro rootfstype=ext2 init=/sbin/linux-init}
kernel=${1:-./fiwix-generic}
launcher=${2:-arch/riscv64/fixture/kaem-seed-init.elf}
root=$(cd "$(dirname "$0")/.." && pwd)
stage0=$(cd "$STAGE0_DIR" && pwd)
completion=${3:-$root/arch/riscv64/fixture/kaem-complete.elf}

case $KAEM_STAGE in
	seed|phase2|phase3|phase4|mini|linux) ;;
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
	elif test "$KAEM_STAGE" = phase3; then
		install -m 644 "$root/tests/fixtures/riscv64-kaem-phase3.kaem" \
			"$rootfs/riscv64/mescc-tools-seed-kaem.kaem"
		install -m 644 \
			"$root/tests/fixtures/riscv64-kaem-phase3-body.kaem" \
			"$rootfs/riscv64/mescc-tools-phase3-kaem.kaem"
	elif test "$KAEM_STAGE" = phase4; then
		install -m 644 "$root/tests/fixtures/riscv64-kaem-phase4.kaem" \
			"$rootfs/riscv64/mescc-tools-seed-kaem.kaem"
		install -m 644 \
			"$root/tests/fixtures/riscv64-kaem-phase4-body.kaem" \
			"$rootfs/riscv64/mescc-tools-phase4-kaem.kaem"
	else
		install -m 644 "$root/tests/fixtures/riscv64-kaem-mini.kaem" \
			"$rootfs/riscv64/mescc-tools-seed-kaem.kaem"
		test -f "$completion" || {
			echo "missing kaem completion fixture: $completion" >&2
			exit 1
		}
		mkdir -p "$rootfs/sbin"
		install -m 755 "$completion" "$rootfs/sbin/kaem-complete"
		if test "$KAEM_STAGE" = linux; then
			test -f "$LINUX_IMAGE" || {
				echo "KAEM_STAGE=linux requires LINUX_IMAGE" >&2
				exit 1
			}
			test -f "$LINUX_INIT" || {
				echo "KAEM_STAGE=linux requires LINUX_INIT" >&2
				exit 1
			}
			install -m 644 "$LINUX_IMAGE" "$rootfs/linux"
			install -m 755 "$LINUX_INIT" "$rootfs/sbin/linux-init"
		fi
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
	if test "$KAEM_STAGE" = linux; then
		set -- "$@" -append "$LINUX_CMDLINE"
	fi
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
	if { test "$KAEM_STAGE" = mini || test "$KAEM_STAGE" = linux; } &&
		test "$status" -ne 0; then
		cat "$serial" >&2
		exit 1
	fi
	if ! grep -q 'Fiwix riscv64 kaem seed launcher entered' "$serial" ||
		grep -q 'kaem seed launcher exec failed\|Subprocess error\|fatal\|PANIC' \
		"$serial"; then
		cat "$serial" >&2
		exit 1
	fi
	if test "$KAEM_STAGE" = mini &&
		! grep -q 'Fiwix riscv64 kaem mini completed' "$serial"; then
		cat "$serial" >&2
		exit 1
	fi
	if test "$KAEM_STAGE" = linux && {
		! grep -q '^Fiwix riscv64 kaem Linux handoff' "$serial" ||
		! grep -q '^Fiwix riscv64 Linux Image header gate passed' "$serial" ||
		! grep -q '^Fiwix riscv64 Linux root handoff' "$serial" ||
		! grep -q '^Linux version ' "$serial" ||
		! grep -q 'VFS: Mounted root (ext2 filesystem) readonly' "$serial" ||
		! grep -q '^Fiwix riscv64 Linux root PID 1 passed' "$serial" ||
		grep -q 'Kernel panic' "$serial"
	}; then
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
	if test "$KAEM_STAGE" = mini || test "$KAEM_STAGE" = linux; then
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
	elif test "$KAEM_STAGE" = phase2 || test "$KAEM_STAGE" = phase3 ||
		test "$KAEM_STAGE" = phase4; then
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
		if test "$KAEM_STAGE" = phase3 || test "$KAEM_STAGE" = phase4; then
			phase3_catm_actual=$work/$name.catm
			phase3_m0_source_actual=$work/$name.M0.hex2
			phase3_m0_actual=$work/$name.M0
			"$DEBUGFS" -R 'cat /riscv64/artifact/catm' "$run_disk" \
				> "$phase3_catm_actual" 2>/dev/null
			"$DEBUGFS" -R 'cat /riscv64/artifact/M0.hex2' "$run_disk" \
				> "$phase3_m0_source_actual" 2>/dev/null
			"$DEBUGFS" -R 'cat /riscv64/artifact/M0' "$run_disk" \
				> "$phase3_m0_actual" 2>/dev/null
			check_hash "$phase3_catm_actual" \
				ad24954282f57a2704c890fd95c375745b076964b6d731f1baf9f387afee2c0b
			check_hash "$phase3_m0_source_actual" \
				a3b3bff7a7aafafae8e3bda4614727b20ac8772e91907811bb1819f91f069d7f
			check_hash "$phase3_m0_actual" \
				495d188c9e24bba8e7c76bcb11cdfd9a6624e037c4df50b07b2aeb4a8f68b3ea
		fi
		if test "$KAEM_STAGE" = phase4; then
			phase4_cc_source_actual=$work/$name.cc_riscv64.M1
			phase4_cc_hex_actual=$work/$name.cc_riscv64.hex2
			phase4_cc_link_actual=$work/$name.cc_riscv64-0.hex2
			phase4_cc_actual=$work/$name.cc_riscv64
			"$DEBUGFS" -R 'cat /riscv64/artifact/cc_riscv64.M1' "$run_disk" \
				> "$phase4_cc_source_actual" 2>/dev/null
			"$DEBUGFS" -R 'cat /riscv64/artifact/cc_riscv64.hex2' "$run_disk" \
				> "$phase4_cc_hex_actual" 2>/dev/null
			"$DEBUGFS" -R 'cat /riscv64/artifact/cc_riscv64-0.hex2' "$run_disk" \
				> "$phase4_cc_link_actual" 2>/dev/null
			"$DEBUGFS" -R 'cat /riscv64/artifact/cc_riscv64' "$run_disk" \
				> "$phase4_cc_actual" 2>/dev/null
			check_hash "$phase4_cc_source_actual" \
				56ba662448370c78e5c5d5ff3d98412d75d30cb625c89d021d7e31c03963e69c
			check_hash "$phase4_cc_hex_actual" \
				17b4317b4e3ad61aa0923ede8cae83cbf3276550f2fa1cd95868e1372a552412
			check_hash "$phase4_cc_link_actual" \
				3c7feea89506b70ceb602257ffc56b2481dcfd474527c473fad022c039a50b64
			check_hash "$phase4_cc_actual" \
				fe337e9c2d9b6e6a550561491b7d2640d088975e107ee9b522641428e1362685
		fi
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
