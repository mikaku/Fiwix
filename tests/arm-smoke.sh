#!/bin/sh
set -eu

QEMU=${QEMU:-qemu-system-arm}
TIMEOUT=${TIMEOUT:-10}
KERNEL=${1:-./fiwix}
LOG=${LOG:-arm-smoke.log}

timeout "$TIMEOUT" "$QEMU" \
    -M virt,virtualization=on,secure=off -cpu cortex-a15 \
    -m 128M -smp 1 -nographic -kernel "$KERNEL" -no-reboot \
    > "$LOG" 2>&1

grep 'Fiwix ARMv7 firmware-free boot' "$LOG"
grep 'mode=0x00000013' "$LOG"
grep 'arm boot smoke passed' "$LOG"
grep 'arm ELF32 load passed' "$LOG"
grep 'arm process page tables passed' "$LOG"
grep 'arm process root switch passed' "$LOG"
grep 'arm scheduler context passed' "$LOG"
grep 'arm vector and timer setup passed' "$LOG"
grep 'Fiwix ARMv7 user SVC passed' "$LOG"
grep 'Fiwix ARMv7 data abort passed' "$LOG"
grep 'Fiwix ARMv7 permission abort passed' "$LOG"
grep 'Fiwix ARMv7 timer IRQ passed' "$LOG"
grep 'arm trap smoke passed' "$LOG"
if grep -Ei 'undefined|unhandled|panic|fail' "$LOG"; then
    printf '%s\n' 'ARM smoke emitted a failure marker' >&2
    exit 1
fi
dtb=$(sed -n 's/.* dtb=0x\([0-9A-F][0-9A-F]*\).*/\1/p' "$LOG")
dtb_value=$((0x$dtb))
if [ "$dtb_value" -lt $((0x40000000)) ] ||
   [ "$dtb_value" -ge $((0x48000000)) ]; then
    printf 'ARM FDT address is outside guest RAM: 0x%s\n' "$dtb" >&2
    exit 1
fi

printf '%s\n' 'ARMv7 boot, SVC, abort, and timer IRQ smoke passed'
