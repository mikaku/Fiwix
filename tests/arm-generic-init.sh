#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

READELF=${READELF:-arm-linux-gnueabihf-readelf}
NM=${NM:-arm-linux-gnueabihf-nm}
INIT=${1:?usage: $0 INIT_ELF}
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

test -f "$INIT"
size=$(wc -c < "$INIT")
if test "$size" -le 0 || test "$size" -gt 12288; then
	printf 'ARM generic init has invalid size: %s\n' "$size" >&2
	exit 1
fi

"$READELF" -h "$INIT" > "$temporary/header"
"$READELF" -lW "$INIT" > "$temporary/programs"
"$READELF" -r "$INIT" > "$temporary/relocations"
"$NM" "$INIT" > "$temporary/symbols"

grep -q 'Class:.*ELF32' "$temporary/header"
grep -q 'Type:.*EXEC' "$temporary/header"
grep -q 'Machine:.*ARM' "$temporary/header"
grep -q 'LOAD.*R E' "$temporary/programs"
if grep -Eq 'INTERP|LOAD.*RWE' "$temporary/programs"; then
	cat "$temporary/programs" >&2
	exit 1
fi
grep -q 'There are no relocations in this file' "$temporary/relocations"
grep -q ' T _start$' "$temporary/symbols"

printf 'Fiwix ARM generic init shape passed: %s bytes\n' "$size"
