# RISC-V 64 port

The experimental riscv64 target boots directly on the single-hart QEMU `virt`
machine. It currently proves the firmware-free M-mode entry, the transition to
S mode, fatal trap reporting, machine-timer forwarding, and the architecture
context-switch primitive with two kernel tasks. It does not yet run the generic
Fiwix scheduler or userspace.

## Build

The bring-up build requires an ELF-capable RISC-V cross toolchain:

```sh
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- clean
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu-
```

The resulting `fiwix` is an ELF64 kernel linked at `0x80000000`. It uses
RV64IMA plus `Zicsr` and `Zifencei`, with floating-point and compressed
instructions disabled.

## Test

```sh
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 test-riscv64
```

QEMU must provide its standard `virt` 16550 UART, CLINT, and test finisher.
The test uses `-bios none`; OpenSBI is not part of the runtime contract.

## Current machine contract

- one hart and 256 MiB RAM;
- direct kernel entry at `0x80000000` with hart ID in `a0` and DTB in `a1`;
- UART at `0x10000000`;
- CLINT `mtimecmp` at `0x02004000` and `mtime` at `0x0200bff8`;
- test finisher at `0x00100000`;
- M mode grants S/U physical access with PMP, delegates synchronous traps, and
  forwards machine timer interrupts as supervisor software interrupts.

The fixed addresses are a deliberate first bring-up boundary. The saved DTB
pointer will become the source of discoverable RAM and devices before the port
is considered hardware-portable.

## Process-context design

The first member of `struct proc` is now an architecture-owned
`struct arch_context`. Its i386 definition preserves the former TSS layout, so
the existing hardware-facing offsets and generated code remain unchanged. The
riscv64 definition stores `ra`, `sp`, the twelve callee-saved registers, `satp`,
and the per-process kernel stack pointer. The switch primitive deliberately
handles only the callee-saved execution state; address-space activation and
trap-stack selection belong at the scheduler boundary.

The smoke kernel initializes two independent kernel stacks and performs six
cooperative switches before returning to the boot context. This gate checks the
assembly offsets against the C structure and gives later generic-scheduler work
a known-good low-level primitive.
