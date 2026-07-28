#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

GENERIC_CC=${GENERIC_CC:-clang}
GENERIC_CC_TARGET=${GENERIC_CC_TARGET:---target=arm-linux-gnueabihf}
GENERIC_LD=${GENERIC_LD:-arm-linux-gnueabihf-ld}
GENERIC_OUTPUT=${GENERIC_OUTPUT:-}
GENERIC_OBJECT_DIR=${GENERIC_OBJECT_DIR:-}
GENERIC_OBJECT_LIST=${GENERIC_OBJECT_LIST:-}
GENERIC_SOURCE_LIST=${GENERIC_SOURCE_LIST:-}
root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
GENERIC_SOURCE_LIST=${GENERIC_SOURCE_LIST:-$root/tests/arm-generic-sources.list}
if [ -n "$GENERIC_OBJECT_DIR" ]; then
	temporary=$GENERIC_OBJECT_DIR
	mkdir -p "$temporary"
else
	temporary=$(mktemp -d)
	trap 'rm -rf "$temporary"' EXIT HUP INT TERM
fi

set -- "$GENERIC_CC"
if [ -n "$GENERIC_CC_TARGET" ]; then
	set -- "$@" "$GENERIC_CC_TARGET"
fi
compiled=0
objects=
cd "$root"

test -f "$GENERIC_SOURCE_LIST"
if [ -n "$GENERIC_OBJECT_LIST" ]; then
	: > "$GENERIC_OBJECT_LIST"
fi

while IFS= read -r source; do
	test -n "$source" || continue
	case "$source" in
		\#*) continue ;;
	esac
	test -f "$source"
	output=$temporary/generic-$compiled.o
	"$@" -march=armv7-a -mfloat-abi=soft -marm \
		-mno-unaligned-access -std=c89 \
		-D__KERNEL__ -DCONFIG_ARCH_ARM -I"$root/include" -O2 \
		-fno-pie -fno-pic -fno-common -fno-stack-protector \
		-ffreestanding -ffunction-sections -fdata-sections \
		-Wall -Wstrict-prototypes -c "$source" -o "$output"
	objects="$objects $output"
	if [ -n "$GENERIC_OBJECT_LIST" ]; then
		printf '%s\n' "$output" >> "$GENERIC_OBJECT_LIST"
	fi
	compiled=$((compiled + 1))
done < "$GENERIC_SOURCE_LIST"

test "$compiled" -eq 268
if [ -n "$GENERIC_OUTPUT" ]; then
	# shellcheck disable=SC2086
	"$GENERIC_LD" -m armelf_linux_eabi -r $objects -o "$GENERIC_OUTPUT"
fi
printf 'Fiwix ARM generic compile gate passed: %s files\n' "$compiled"
