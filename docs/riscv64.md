# RISC-V 64 port

The experimental riscv64 target boots directly on the single-hart QEMU `virt`
machine. It currently proves the firmware-free M-mode entry, the transition to
S mode, fatal trap reporting, machine-timer forwarding, and the architecture
context-switch primitive with two kernel tasks, Sv39 address translation, and a
small U-mode RV64 syscall fixture, and polled virtio-mmio block reads. It does
not yet run the generic Fiwix scheduler or a filesystem-backed userspace.

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

## Sv39 and U-mode design

The bring-up page tables keep a supervisor-only 1 GiB identity mapping for the
kernel and add supervisor-only low leaves for the QEMU UART and test finisher.
Two 4 KiB user leaves map the fixture text read/execute and its stack
read/write. No user mapping is writable and executable at the same time.

The supervisor trap entry uses `sscratch` to distinguish U-mode traps from
S-mode interrupts without destroying a general register. User traps run on a
dedicated 8 KiB kernel stack and save all integer registers, `sepc`, `sstatus`,
and `stval`. The initial RV64 syscall dispatcher implements Linux-compatible
numbers 64 (`write`) and 93 (`exit`); it validates the fixture's output buffer
before temporarily reading its mapped page with `SUM` enabled. The smoke test
requires a U-mode `write` marker and an `exit(42)` return to the suspended
supervisor context.

The build produces a standalone ELF64/RISC-V executable at
`arch/riscv64/fixture/user.elf` and embeds its bytes as immutable loader input.
The kernel validates the ELF identity, machine, executable type, program-header
bounds, one-page RX load segment, alignment, entry point, and W^X policy before
copying the segment into its user page. The fixture then verifies an
ABI-shaped initial stack containing `argc`, `argv`, a null environment, and
`AT_PAGESZ`, `AT_ENTRY`, and `AT_NULL` auxiliary-vector entries.

Embedding the standalone ELF is intentionally not treated as the final
filesystem path. It is a deterministic loader/privilege/MMU/trap gate that must
remain green while generic process state and filesystem-backed binaries are
added.

## Storage bring-up

The first storage gate scans the QEMU `virt` MMIO window rather than assuming a
device slot, negotiates an eight-entry-or-smaller split queue, and reads sector
zero from a generated read-only disk fixture. The same test runs twice: once
with QEMU's legacy virtio-mmio v1 transport and once with modern v2. Queue
completion is polled with a finite bound, and the driver verifies both the
virtio request status and exact sector marker before reporting success.

This queue code is not yet registered with Fiwix's generic block layer. Its
purpose is to prove discovery, feature negotiation, DMA addresses, both queue
layouts, and sector I/O before adapting the generic buffer cache and ext2 mount
path.
