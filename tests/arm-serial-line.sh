#!/bin/sh
# Copyright 2026, Fiwix ARM contributors.
# Distributed under the terms of the Fiwix License.

fiwix_arm_serial_has_line()
{
	line=$1
	log=$2
	cr=$(printf '\r')
	grep -Fqx "$line" "$log" || grep -Fqx "$line$cr" "$log"
}
