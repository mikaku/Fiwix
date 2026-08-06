#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

root=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
. "$root/tests/arm-serial-line.sh"

temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
log=$temporary/serial.log

printf 'seed entered\nseed compiled\r\nseed completed\r\n' > "$log"
fiwix_arm_serial_has_line 'seed entered' "$log"
fiwix_arm_serial_has_line 'seed compiled' "$log"
fiwix_arm_serial_has_line 'seed completed' "$log"

if fiwix_arm_serial_has_line 'seed' "$log" ||
	fiwix_arm_serial_has_line 'seed completed extra' "$log" ||
	fiwix_arm_serial_has_line 'seed absent' "$log"; then
	echo 'serial line matcher accepted a non-exact marker' >&2
	exit 1
fi

sed 's/seed completed/seed incomplete/' "$log" > "$temporary/ablated.log"
if fiwix_arm_serial_has_line 'seed completed' "$temporary/ablated.log"; then
	echo 'serial line matcher missed the ablated completion marker' >&2
	exit 1
fi

echo 'Fiwix ARM serial line self-test passed'
