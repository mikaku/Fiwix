#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

ARMCC=${ARMCC:-clang}
ARMCC_TARGET=${ARMCC_TARGET:---target=arm-linux-gnueabihf}
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

set -- "$ARMCC"
if [ -n "$ARMCC_TARGET" ]; then
	set -- "$@" "$ARMCC_TARGET"
fi
"$@" -std=c89 -Wall -Wextra -Werror -c \
	"$root/tests/arm-signal-uapi.c" \
	-o "$temporary/arm-signal-uapi.o"

printf '%s\n' 'Fiwix ARM Linux signal-UAPI layout gate passed'
