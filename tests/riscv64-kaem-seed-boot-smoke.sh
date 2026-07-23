#!/bin/sh

set -eu

QEMU=${QEMU:-qemu-system-riscv64}
TIMEOUT=${TIMEOUT:-20}
MKE2FS=${MKE2FS:-}
DEBUGFS=${DEBUGFS:-}
SHA256SUM=${SHA256SUM:-sha256sum}
TAR=${TAR:-tar}
STAGE0_DIR=${STAGE0_DIR:?set STAGE0_DIR to the stage0-posix checkout}
KAEM_WORKDIR=${KAEM_WORKDIR:-}
KAEM_STAGE=${KAEM_STAGE:-seed}
KAEM_TRANSPORT=${KAEM_TRANSPORT:-all}
LIVE_BOOTSTRAP_DIR=${LIVE_BOOTSTRAP_DIR:-}
LIVE_BOOTSTRAP_DISTFILES=${LIVE_BOOTSTRAP_DISTFILES:-}
QEMU_MEMORY=${QEMU_MEMORY:-}
LINUX_IMAGE=${LINUX_IMAGE:-}
LINUX_INIT=${LINUX_INIT:-}
LINUX_CMDLINE=${LINUX_CMDLINE:-}
kernel=${1:-./fiwix-generic}
launcher=${2:-arch/riscv64/fixture/kaem-seed-init.elf}
root=$(cd "$(dirname "$0")/.." && pwd)
stage0=$(cd "$STAGE0_DIR" && pwd)
completion=${3:-$root/arch/riscv64/fixture/kaem-complete.elf}

if test -z "$MKE2FS"; then
	MKE2FS=mke2fs
	if ! command -v "$MKE2FS" >/dev/null 2>&1; then
		for candidate in /usr/sbin/mke2fs /sbin/mke2fs; do
			if test -x "$candidate"; then
				MKE2FS=$candidate
				break
			fi
		done
	fi
fi
if test -z "$DEBUGFS"; then
	DEBUGFS=debugfs
	if ! command -v "$DEBUGFS" >/dev/null 2>&1; then
		for candidate in /usr/sbin/debugfs /sbin/debugfs; do
			if test -x "$candidate"; then
				DEBUGFS=$candidate
				break
			fi
		done
	fi
fi
command -v "$MKE2FS" >/dev/null
command -v "$DEBUGFS" >/dev/null

case $KAEM_STAGE in
	seed|phase2|phase3|phase4|mini|manifest1|manifest2|manifest3|manifest4|manifest5|linux|stage0-linux) ;;
	*) echo "unsupported KAEM_STAGE: $KAEM_STAGE" >&2; exit 1 ;;
esac
linux_boot=false
if test "$KAEM_STAGE" = linux || test "$KAEM_STAGE" = stage0-linux; then
	linux_boot=true
fi
if test -z "$LINUX_CMDLINE"; then
	if test "$KAEM_STAGE" = stage0-linux; then
		LINUX_CMDLINE='earlycon=uart8250,mmio,0x10000000 console=ttyS0,115200 root=/dev/vda rw rootfstype=ext2 init=/sbin/linux-init'
	else
		LINUX_CMDLINE='earlycon=uart8250,mmio,0x10000000 console=ttyS0,115200 root=/dev/vda ro rootfstype=ext2 init=/sbin/linux-init'
	fi
fi
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
extract_guest_file()
{
	guest_path=$1
	disk_path=$2
	output_path=$3
	metadata=$output_path.stat
	"$DEBUGFS" -R "stat $guest_path" "$disk_path" \
		> "$metadata" 2>/dev/null
	if ! grep -q 'Type: regular' "$metadata"; then
		echo "missing regular guest file: $guest_path" >&2
		return 1
	fi
	rm -f "$metadata"
	"$DEBUGFS" -R "cat $guest_path" "$disk_path" \
		> "$output_path" 2>/dev/null
}
check_hash "$hex0_seed" \
	1b50ceef632b83b79aef0cf91d60bc0cb242a3b2bfba22cb5115d80112b50ac9
check_hash "$kaem_seed" \
	12c0a7d01f2e369598ffdfc6e0881d62d20621d5f0d9fa9580ee511d72300650

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
	while read -r revision path _; do
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
	if test "$KAEM_STAGE" = stage0-linux; then
		seed_fixture=$root/tests/fixtures/riscv64-kaem-linux-continuation.kaem
		mkdir -p "$rootfs/steps"
		install -m 644 "$seed_fixture" \
			"$rootfs/riscv64/mescc-tools-seed-kaem.kaem"
		install -m 644 \
			"$root/tests/fixtures/riscv64-live-bootstrap-fiwix-handoff.kaem" \
			"$rootfs/steps/live-bootstrap-fiwix.kaem"
		install -m 644 \
			"$root/tests/fixtures/riscv64-live-bootstrap-stage0-linux.kaem" \
			"$rootfs/steps/live-bootstrap-linux.kaem"
		linux_answers=$root/tests/fixtures/riscv64-linux.answers
		while IFS= read -r answer; do
			grep -Fqx "$answer" "$rootfs/riscv64.answers" || {
				echo "noncanonical Linux answer: $answer" >&2
				exit 1
			}
		done < "$linux_answers"
		install -m 644 "$linux_answers" "$rootfs/riscv64-linux.answers"
	elif test "$KAEM_STAGE" = phase2; then
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
	elif test "$KAEM_STAGE" = manifest1 || test "$KAEM_STAGE" = manifest2 ||
		test "$KAEM_STAGE" = manifest3 || test "$KAEM_STAGE" = manifest4 ||
		test "$KAEM_STAGE" = manifest5; then
		git -C "$LIVE_BOOTSTRAP_DIR" rev-parse --git-dir >/dev/null 2>&1 || {
			echo "KAEM_STAGE=$KAEM_STAGE requires LIVE_BOOTSTRAP_DIR" >&2
			exit 1
		}
		expected_live_bootstrap_commit=9a268c4c39cae952b268bc86da342be2175f03d4
		actual_live_bootstrap_commit=$(git -C "$LIVE_BOOTSTRAP_DIR" rev-parse HEAD)
		test "$actual_live_bootstrap_commit" = "$expected_live_bootstrap_commit" || {
			echo "live-bootstrap checkout is not at $expected_live_bootstrap_commit" >&2
			exit 1
		}
		live_bootstrap_source=$work/live-bootstrap-source
		mkdir -p "$live_bootstrap_source"
		set -- steps/checksum-transcriber-1.0
		if test "$KAEM_STAGE" = manifest2 || test "$KAEM_STAGE" = manifest3 ||
			test "$KAEM_STAGE" = manifest4 || test "$KAEM_STAGE" = manifest5; then
			set -- "$@" steps/simple-patch-1.0
		fi
		if test "$KAEM_STAGE" = manifest3 || test "$KAEM_STAGE" = manifest4 ||
			test "$KAEM_STAGE" = manifest5; then
			set -- "$@" steps/mes-0.27.1
		fi
		if test "$KAEM_STAGE" = manifest4 || test "$KAEM_STAGE" = manifest5; then
			set -- "$@" steps/tcc-0.9.26
		fi
		git -C "$LIVE_BOOTSTRAP_DIR" archive --format=tar \
			"$expected_live_bootstrap_commit" "$@" | \
			"$TAR" -xf - -C "$live_bootstrap_source"
		install -m 644 \
			"$root/tests/fixtures/riscv64-kaem-$KAEM_STAGE.kaem" \
			"$rootfs/riscv64/mescc-tools-seed-kaem.kaem"
		mkdir -p "$rootfs/steps/checksum-transcriber-1.0/src"
		install -m 644 \
			"$root/tests/fixtures/riscv64-live-bootstrap-$KAEM_STAGE.kaem" \
			"$rootfs/steps/live-bootstrap-$KAEM_STAGE.kaem"
		for input in pass1.kaem \
			checksum-transcriber-1.0.riscv64.checksums; do
			install -m 644 \
				"$live_bootstrap_source/steps/checksum-transcriber-1.0/$input" \
				"$rootfs/steps/checksum-transcriber-1.0/$input"
		done
		install -m 644 \
			"$live_bootstrap_source/steps/checksum-transcriber-1.0/src/checksum-transcriber.c" \
			"$rootfs/steps/checksum-transcriber-1.0/src/checksum-transcriber.c"
		if test "$KAEM_STAGE" = manifest2 || test "$KAEM_STAGE" = manifest3 ||
			test "$KAEM_STAGE" = manifest4 || test "$KAEM_STAGE" = manifest5; then
			mkdir -p "$rootfs/steps/simple-patch-1.0/src"
			for input in pass1.kaem simple-patch-1.0.riscv64.checksums; do
				install -m 644 \
					"$live_bootstrap_source/steps/simple-patch-1.0/$input" \
					"$rootfs/steps/simple-patch-1.0/$input"
			done
			install -m 644 \
				"$live_bootstrap_source/steps/simple-patch-1.0/src/simple-patch.c" \
				"$rootfs/steps/simple-patch-1.0/src/simple-patch.c"
		fi
		if test "$KAEM_STAGE" = manifest3 || test "$KAEM_STAGE" = manifest4 ||
			test "$KAEM_STAGE" = manifest5; then
			test -d "$LIVE_BOOTSTRAP_DISTFILES" || {
				echo "KAEM_STAGE=$KAEM_STAGE requires LIVE_BOOTSTRAP_DISTFILES" >&2
				exit 1
			}
			mes_distfile=$LIVE_BOOTSTRAP_DISTFILES/mes-0.27.1.tar.gz
			nyacc_distfile=$LIVE_BOOTSTRAP_DISTFILES/nyacc-1.00.2-lb1.tar.gz
			check_hash "$mes_distfile" \
				183a40ea47ea49f8a1e3bd1b9d12e676374d64d63bc79e7bc1ae7d673dfdf25d
			check_hash "$nyacc_distfile" \
				708c943f89c972910e9544ee077771acbd0a2c0fc6d33496fe158264ddb65327
			mkdir -p "$rootfs/distfiles"
			install -m 644 "$mes_distfile" "$rootfs/distfiles/mes-0.27.1.tar.gz"
			install -m 644 "$nyacc_distfile" \
				"$rootfs/distfiles/nyacc-1.00.2-lb1.tar.gz"
			"$TAR" -C "$live_bootstrap_source" -cf "$work/mes-package.tar" \
				steps/mes-0.27.1
			"$TAR" -C "$rootfs" -xf "$work/mes-package.tar"
		fi
		if test "$KAEM_STAGE" = manifest4 || test "$KAEM_STAGE" = manifest5; then
			tcc_distfile=$LIVE_BOOTSTRAP_DISTFILES/tcc-0.9.26-1157-gdd46e018.tar.gz
			check_hash "$tcc_distfile" \
				3748c0aacd1e7b3805de09f28a4ef396392b2c838f78c59d23bfd9d68312232e
			install -m 644 "$tcc_distfile" \
				"$rootfs/distfiles/tcc-0.9.26-1157-gdd46e018.tar.gz"
			mkdir -p "$rootfs/steps/tcc-0.9.26"
			for fixture in pass1.kaem compile.kaem runtime.kaem \
				unified-libc.kaem config.h sources.SHA256SUM; do
				install -m 644 \
					"$root/tests/fixtures/riscv64-tcc-0.9.26-$fixture" \
					"$rootfs/steps/tcc-0.9.26/$fixture"
			done
		fi
		if test "$KAEM_STAGE" = manifest5; then
			tcc_mob_commit=923fba83f1e541750c4dd48a4ec02af831ee5af8
			tcc_mob_distfile=$LIVE_BOOTSTRAP_DISTFILES/tcc-mob-$tcc_mob_commit.tar.gz
			check_hash "$tcc_mob_distfile" \
				aec6a2a3e2b1b2c8c5a8507a0677a6556b9b20a55e4af17d8aa6a04e7cb75a45
			install -m 644 "$tcc_mob_distfile" \
				"$rootfs/distfiles/tcc-mob-$tcc_mob_commit.tar.gz"
			mkdir -p "$rootfs/steps/tcc-mob"
			install -m 644 \
				"$root/tests/fixtures/riscv64-tcc-mob-pass1.kaem" \
				"$rootfs/steps/tcc-mob/pass1.kaem"
			install -m 644 \
				"$root/tests/fixtures/riscv64-tcc-mob-sources.SHA256SUM" \
				"$rootfs/steps/tcc-mob/sources.SHA256SUM"
			install -m 644 \
				"$root/tests/fixtures/riscv64-tcc-mob-smoke.c" \
				"$rootfs/steps/tcc-mob/smoke.c"
			install -m 644 \
				"$root/tests/fixtures/riscv64-tcc-mob-decimal.c" \
				"$rootfs/steps/tcc-mob/decimal.c"
			for fixture in static-link.before static-link.after \
				ldexpl-helper.before ldexpl-helper.after \
				ldexpl-use.before ldexpl-use.after \
				ar-helper.before ar-helper.after ar-use.before \
				ar-use.after abtod.before abtod.after; do
				install -m 644 \
					"$root/tests/fixtures/riscv64-tcc-mob-$fixture" \
					"$rootfs/steps/tcc-mob/$fixture"
			done
		fi
	else
		install -m 644 "$root/tests/fixtures/riscv64-kaem-mini.kaem" \
			"$rootfs/riscv64/mescc-tools-seed-kaem.kaem"
	fi
	if test "$linux_boot" = true; then
		test -f "$LINUX_IMAGE" || {
			echo "KAEM_STAGE=$KAEM_STAGE requires LINUX_IMAGE" >&2
			exit 1
		}
		test -f "$LINUX_INIT" || {
			echo "KAEM_STAGE=$KAEM_STAGE requires LINUX_INIT" >&2
			exit 1
		}
		mkdir -p "$rootfs/sbin"
		install -m 644 "$LINUX_IMAGE" "$rootfs/linux"
		install -m 755 "$LINUX_INIT" "$rootfs/sbin/linux-init"
		if test "$KAEM_STAGE" = stage0-linux; then
			handoff=$root/arch/riscv64/fixture/kaem-linux-complete.elf
			test -f "$handoff" || {
				echo "missing Fiwix-to-Linux handoff fixture: $handoff" >&2
				exit 1
			}
			install -m 755 "$handoff" "$rootfs/sbin/fiwix-linux-handoff"
		fi
	fi
	if test "$KAEM_STAGE" = mini || test "$KAEM_STAGE" = manifest1 ||
		test "$KAEM_STAGE" = manifest2 ||
		test "$KAEM_STAGE" = manifest3 ||
		test "$KAEM_STAGE" = manifest4 ||
		test "$KAEM_STAGE" = manifest5 ||
		test "$KAEM_STAGE" = linux ||
		test "$KAEM_STAGE" = stage0-linux; then
		test -f "$completion" || {
			echo "missing kaem completion fixture: $completion" >&2
			exit 1
		}
		mkdir -p "$rootfs/sbin"
		install -m 755 "$completion" "$rootfs/sbin/kaem-complete"
	fi
	if test "$KAEM_STAGE" = manifest3 || test "$KAEM_STAGE" = manifest4 ||
		test "$KAEM_STAGE" = manifest5; then
		disk_blocks=262144
		if test "$KAEM_STAGE" = manifest5; then
			disk_blocks=524288
		fi
		qemu_memory=${QEMU_MEMORY:-2G}
	else
		disk_blocks=131072
		qemu_memory=${QEMU_MEMORY:-256M}
	fi
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
	qemu_memory=${QEMU_MEMORY:-256M}
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
	if test "$linux_boot" = true; then
		set -- "$@" -append "$LINUX_CMDLINE"
	fi
	if timeout "$TIMEOUT" "$QEMU" -machine virt -m "$qemu_memory" -smp 1 \
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
	if { test "$KAEM_STAGE" = mini || test "$KAEM_STAGE" = manifest1 ||
		test "$KAEM_STAGE" = manifest2 ||
		test "$KAEM_STAGE" = manifest3 ||
		test "$KAEM_STAGE" = manifest4 ||
		test "$KAEM_STAGE" = manifest5 ||
		test "$KAEM_STAGE" = linux ||
		test "$KAEM_STAGE" = stage0-linux; } &&
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
	if test "$KAEM_STAGE" = manifest1 &&
		! grep -q '^Fiwix riscv64 live-bootstrap manifest 1 completed' \
		"$serial"; then
		cat "$serial" >&2
		exit 1
	fi
	if test "$KAEM_STAGE" = manifest2 &&
		! grep -q '^Fiwix riscv64 live-bootstrap manifest 2 completed' \
		"$serial"; then
		cat "$serial" >&2
		exit 1
	fi
	if test "$KAEM_STAGE" = manifest3 &&
		! grep -q '^Fiwix riscv64 live-bootstrap manifest 3 completed' \
		"$serial"; then
		cat "$serial" >&2
		exit 1
	fi
	if test "$KAEM_STAGE" = manifest4 &&
		! grep -q '^Fiwix riscv64 live-bootstrap manifest 4 completed' \
		"$serial"; then
		cat "$serial" >&2
		exit 1
	fi
	if test "$KAEM_STAGE" = manifest5 &&
		! grep -q '^Fiwix riscv64 live-bootstrap manifest 5 completed' \
		"$serial"; then
		cat "$serial" >&2
		exit 1
	fi
	if test "$linux_boot" = true && {
		! grep -q '^Fiwix riscv64 kaem Linux handoff' "$serial" ||
		! grep -q '^Fiwix riscv64 Linux Image header gate passed' "$serial" ||
		! grep -q '^Fiwix riscv64 Linux root handoff' "$serial" ||
		! grep -q '^Linux version ' "$serial" ||
		! grep -q 'VFS: Mounted root (ext2 filesystem)' "$serial" ||
		grep -q 'Kernel panic' "$serial"
	}; then
		cat "$serial" >&2
		exit 1
	fi
	if test "$KAEM_STAGE" = linux &&
		! grep -q '^Fiwix riscv64 Linux root PID 1 passed' "$serial"; then
		cat "$serial" >&2
		exit 1
	fi
	if test "$KAEM_STAGE" = stage0-linux &&
		! grep -q '^Fiwix riscv64 Linux stage0 continuation entered' \
			"$serial"; then
		cat "$serial" >&2
		exit 1
	fi
	if test "$KAEM_STAGE" = stage0-linux &&
		! grep -q '^Fiwix riscv64 Linux stage0 re-verification completed' \
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
	if test "$KAEM_STAGE" = mini || test "$KAEM_STAGE" = manifest1 ||
		test "$KAEM_STAGE" = manifest2 ||
		test "$KAEM_STAGE" = manifest3 ||
		test "$KAEM_STAGE" = manifest4 ||
		test "$KAEM_STAGE" = manifest5 ||
		test "$KAEM_STAGE" = linux ||
		test "$KAEM_STAGE" = stage0-linux; then
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
	if test "$KAEM_STAGE" = manifest1 || test "$KAEM_STAGE" = manifest2 ||
		test "$KAEM_STAGE" = manifest3 || test "$KAEM_STAGE" = manifest4 ||
		test "$KAEM_STAGE" = manifest5; then
		manifest1_actual=$work/$name.checksum-transcriber
		"$DEBUGFS" -R 'cat /usr/bin/checksum-transcriber' "$run_disk" \
			> "$manifest1_actual" 2>/dev/null
		check_hash "$manifest1_actual" \
			1c3021d8051fefd615edb50907e3015d810f974b5b9461f8f9aa383478620a0d
	fi
	if test "$KAEM_STAGE" = manifest2 || test "$KAEM_STAGE" = manifest3 ||
		test "$KAEM_STAGE" = manifest4 || test "$KAEM_STAGE" = manifest5; then
		manifest2_actual=$work/$name.simple-patch
		"$DEBUGFS" -R 'cat /usr/bin/simple-patch' "$run_disk" \
			> "$manifest2_actual" 2>/dev/null
		check_hash "$manifest2_actual" \
			dc72b76c8835b1a08b1ecaa2ab8e9179c290805dd2c8bf3636004f375948c238
	fi
	if test "$KAEM_STAGE" = manifest3 || test "$KAEM_STAGE" = manifest4 ||
		test "$KAEM_STAGE" = manifest5; then
		while read -r expected guest_path artifact; do
			actual=$work/$name.$artifact
			extract_guest_file "$guest_path" "$run_disk" "$actual"
			check_hash "$actual" "$expected"
		done <<'EOF'
132066ae1e8fc55c3bd256623d918b86a6dcec2bc6379e2f273f2733fb7f57be /usr/bin/mes-m2 mes-m2
11f33b019f78c90fcbd2385ebb037ee4e36984117799207497c3c83e8d537d1d /usr/bin/mescc.scm mescc.scm
50441b03b915bd51fb811749901a56b3c42186b45f7f466dbf23337eafad520c /usr/lib/riscv64-mes/crt1.s crt1.s
a96a0a8f1b2dd1e5a7dad8264c92b5448be7d29aa7706d40d67be978d5ddc305 /usr/lib/riscv64-mes/crt1.o crt1.o
1511e99da81caa02490078c7a880ac97d439b6fec99cf846dc2044468e2444b8 /usr/lib/riscv64-mes/riscv64.M1 riscv64.M1
ea93d84ef2e599b27b463ba1218836d4abf38873f6a67e5412e8f32096c954e5 /usr/lib/riscv64-mes/libmescc.s libmescc.s
b448e95afd22a07fea99b0b4a21ae5ef6c3e22d9f20e6b6a8c81fb9daeb3e5ee /usr/lib/riscv64-mes/libc+tcc.s libc+tcc.s
31e81fc37dc0c4f9ee0feeba011d29dea1b1a8e84a766c34d79682174c52e281 /usr/lib/riscv64-mes/libc.s libc.s
0edca3696ee26a869e31ed55f43ab084a4063f8dc62438c1902d04b17de2e6ab /usr/lib/riscv64-mes/libmescc.a libmescc.a
e1822748703bf89714f876c2527db8c020b3fb44ec64ff8d93ed55a1922dcda9 /usr/lib/riscv64-mes/libc+tcc.a libc+tcc.a
d7be5dac4a1d11055f830a65d3373ddfcf7c6f1f6c12cec0e47501203fa10bc6 /usr/lib/riscv64-mes/libc.a libc.a
22ad5f7b6e5ea07b275619956bddd913b061d6ad492a442e4b6b2f28898e50ae /usr/lib/linux/riscv64-mes/elf64-header.hex2 elf64-header.hex2
94c796cb34a6e581491d0cf609e7fad01715c84a17b8b2017178a36568a80e48 /usr/lib/linux/riscv64-mes/elf64-footer-single-main.hex2 elf64-footer-single-main.hex2
EOF
	fi
	if test "$KAEM_STAGE" = manifest4 || test "$KAEM_STAGE" = manifest5; then
		while read -r expected guest_path artifact; do
			actual=$work/$name.$artifact
			extract_guest_file "$guest_path" "$run_disk" "$actual"
			check_hash "$actual" "$expected"
		done <<'EOF'
bff3b8f34dcbc57475d5cdd1bfda7c790bd2c9573c919e903957721e60ed002c /usr/bin/tcc-mes tcc-mes
330965425b1410b373d1e6f43422fff73426b4435d8809d4b88affc342df129f /usr/bin/tcc-boot0 tcc-boot0
4dec33f2526cfc78ba823e40c857180d3fed1ac2ac4f993c6fe27d5f1fc7f636 /usr/bin/tcc-boot1 tcc-boot1
dac055050305d336943176c92cc7b46a529d99a102930fa4b981db0a204621f8 /usr/bin/tcc tcc
dac055050305d336943176c92cc7b46a529d99a102930fa4b981db0a204621f8 /usr/bin/tcc-boot5 tcc-boot5
dac055050305d336943176c92cc7b46a529d99a102930fa4b981db0a204621f8 /usr/bin/tcc-boot6 tcc-boot6
328f78c7a0143c0336df95ebe4d7bcf82dd43f5686141f2181542a379278b794 /usr/lib/mes/libc.a tcc-libc.a
a814fbf14822b22447730e8fcd8e6769e71c867ab15a2ed10ac2d6afb960eac4 /usr/lib/mes/libgetopt.a libgetopt.a
4717d74cca709ddccfa9ecd95a16aa9c35645edb69be6de393b87483aca253d6 /usr/lib/mes/crt1.o tcc-crt1.o
441299442aaff2ab97626bf57bfe54d04b56e5e76893ff0f164514b70abb504b /usr/lib/mes/crti.o tcc-crti.o
a4ce786a49fa20a61ee1ca1174436f758e42dbf0502378c4c5c09b97a3dfd571 /usr/lib/mes/crtn.o tcc-crtn.o
9d6490529e28e66abe85bbe720f0ef97d94d240a75fd3dbb182fb6c1a0544a67 /usr/lib/mes/tcc/libtcc1.a libtcc1.a
9aa300aae3fe00e4e2270216bdeea96480f797c8e63205c5c09ad44012e67be0 /steps/tcc-0.9.26/build/tcc-0.9.26-1157-gdd46e018/libtcc1-boot5.o libtcc1-boot5.o
9aa300aae3fe00e4e2270216bdeea96480f797c8e63205c5c09ad44012e67be0 /steps/tcc-0.9.26/build/tcc-0.9.26-1157-gdd46e018/libtcc1-boot6.o libtcc1-boot6.o
4930b25f87a20c82ffd027ce333ff20cd268a8909a0b41edd99bc7b3b83b78e9 /steps/tcc-0.9.26/build/tcc-0.9.26-1157-gdd46e018/lib-arm64-boot5.o lib-arm64-boot5.o
4930b25f87a20c82ffd027ce333ff20cd268a8909a0b41edd99bc7b3b83b78e9 /steps/tcc-0.9.26/build/tcc-0.9.26-1157-gdd46e018/lib-arm64-boot6.o lib-arm64-boot6.o
EOF
	fi
	if test "$KAEM_STAGE" = manifest5; then
		while read -r expected guest_path artifact; do
			actual=$work/$name.$artifact
			extract_guest_file "$guest_path" "$run_disk" "$actual"
			check_hash "$actual" "$expected"
		done <<'EOF'
559380cc6155a2b06f1fed1c1c5d46fb9029fe567633e6e142aeba17d7c240c9 /usr/bin/tcc-mob tcc-mob
618a5f65985beb751a677a32a66a215fd3d1f40f5a9878142063255ddb8db6ae /usr/lib/tcc-mob/libtcc1.a tcc-mob-libtcc1.a
618a5f65985beb751a677a32a66a215fd3d1f40f5a9878142063255ddb8db6ae /usr/lib/tcc-mob/tcc/libtcc1.a tcc-mob-tcc-libtcc1.a
b3bc954df2ba9732c7656c14e9876968493c6c4a08c55e435af6d155317a10e7 /usr/lib/tcc-mob/libc.a tcc-mob-libc.a
ed4b7bb4021302ff03dcdd7fdb0c4be5216580b5c993290ecd7b469725b44328 /usr/lib/tcc-mob/libgetopt.a tcc-mob-libgetopt.a
6c3a74fa5494eb407160c0c97439fda27c08977ddcdba491314a7ba509488221 /usr/lib/tcc-mob/crt1.o tcc-mob-crt1.o
441299442aaff2ab97626bf57bfe54d04b56e5e76893ff0f164514b70abb504b /usr/lib/tcc-mob/crti.o tcc-mob-crti.o
a4ce786a49fa20a61ee1ca1174436f758e42dbf0502378c4c5c09b97a3dfd571 /usr/lib/tcc-mob/crtn.o tcc-mob-crtn.o
559380cc6155a2b06f1fed1c1c5d46fb9029fe567633e6e142aeba17d7c240c9 /steps/tcc-mob/build/tinycc-923fba83f1e541750c4dd48a4ec02af831ee5af8/tcc-mob-stage4 tcc-mob-stage4
559380cc6155a2b06f1fed1c1c5d46fb9029fe567633e6e142aeba17d7c240c9 /steps/tcc-mob/build/tinycc-923fba83f1e541750c4dd48a4ec02af831ee5af8/tcc-mob-stage5 tcc-mob-stage5
0296de667ae8f49fedf69dbdbc34505e6fe5fda5c6b6e51fc046c772458de141 /steps/tcc-mob/build/tinycc-923fba83f1e541750c4dd48a4ec02af831ee5af8/libc-stage3.o tcc-mob-libc-stage3.o
0296de667ae8f49fedf69dbdbc34505e6fe5fda5c6b6e51fc046c772458de141 /steps/tcc-mob/build/tinycc-923fba83f1e541750c4dd48a4ec02af831ee5af8/libc-stage4.o tcc-mob-libc-stage4.o
a4fb54c25b25fea0dcfddd8ecb1021b445f7a3c0c5b9392a1afe2b2ec827bf95 /steps/tcc-mob/build/tinycc-923fba83f1e541750c4dd48a4ec02af831ee5af8/libtcc1-stage3.o tcc-mob-libtcc1-stage3.o
a4fb54c25b25fea0dcfddd8ecb1021b445f7a3c0c5b9392a1afe2b2ec827bf95 /steps/tcc-mob/build/tinycc-923fba83f1e541750c4dd48a4ec02af831ee5af8/libtcc1-stage4.o tcc-mob-libtcc1-stage4.o
3a30f95e1c8f9fe94c43e74c7910d76178d94689494f8f8204a9afc5e23142cb /steps/tcc-mob/build/tinycc-923fba83f1e541750c4dd48a4ec02af831ee5af8/lib-arm64-stage3.o tcc-mob-lib-arm64-stage3.o
3a30f95e1c8f9fe94c43e74c7910d76178d94689494f8f8204a9afc5e23142cb /steps/tcc-mob/build/tinycc-923fba83f1e541750c4dd48a4ec02af831ee5af8/lib-arm64-stage4.o tcc-mob-lib-arm64-stage4.o
EOF
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
