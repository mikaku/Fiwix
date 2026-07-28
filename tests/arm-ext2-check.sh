#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

set -eu

DISK=${1:?usage: $0 DISK}
E2FSCK=${E2FSCK:-}

if test -z "$E2FSCK"; then
	E2FSCK=$(command -v e2fsck 2>/dev/null || true)
fi
if test -z "$E2FSCK" && test -x /sbin/e2fsck; then
	E2FSCK=/sbin/e2fsck
fi
if test -z "$E2FSCK"; then
	exit 0
fi

output=$(mktemp)
trap 'rm -f "$output"' EXIT HUP INT TERM

if ! "$E2FSCK" -fn "$DISK" >"$output" 2>&1; then
	cat "$output" >&2
	exit 1
fi
