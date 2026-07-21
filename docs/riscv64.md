# RISC-V 64 port

The experimental riscv64 target boots directly on the single-hart QEMU `virt`
machine. It currently proves the firmware-free M-mode entry, the transition to
S mode, fatal trap reporting, machine-timer forwarding, and the architecture
context-switch primitive with two kernel tasks, Sv39 address translation, and a
small U-mode RV64 syscall fixture, polled virtio-mmio block reads, and a
read-only ext2 file lookup. It does not yet run the generic Fiwix scheduler or
a filesystem-backed userspace process.

## Build

The bring-up build requires an ELF-capable RISC-V cross toolchain:

```sh
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- clean
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu-
```

The resulting `fiwix` is an ELF64 kernel linked at `0x80000000`. It uses
RV64IMA plus `Zicsr` and `Zifencei`, with floating-point and compressed
instructions disabled.

The riscv64 C target also builds with the bootstrap TinyCC while GNU binutils
assemble and link the architecture files:

```sh
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  TCC=/path/to/riscv64-tcc QEMU=/path/to/qemu-system-riscv64 \
  test-riscv64-tcc
```

This gate passes with both the Mes-built riscv64 `tcc-boot0` 0.9.27 and the
final `tcc-musl` 0.9.28rc artifacts from the bootstrap chain. `TCC_LIBTCC1`
can override the compiler-runtime archive when a package uses a different
layout. The linker accepts TinyCC's LP64D object marker because the kernel C
surface has no floating-point types or calls; the assembly-first final ELF is
marked soft-float, and compile-time assertions require 64-bit pointers and
`unsigned long`.

## Test

```sh
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 test-riscv64

# Exercise direct, single-indirect, and double-indirect ext2 file blocks.
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 test-riscv64-large-image
```

QEMU must provide its standard `virt` 16550 UART, CLINT, and test finisher.
The test uses `-bios none`; OpenSBI is not part of the runtime contract.

An optional real-Linux oracle is reproducible from a Linux source tree:

```sh
JOBS=8 tests/build-riscv64-linux.sh /path/to/linux-6.12.1 /tmp/linux-build
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 \
  LINUX_IMAGE=/tmp/linux-build/arch/riscv/boot/Image test-riscv64-linux
```

The recorded oracle uses upstream Linux 6.12.1 tarball SHA-256
`0193b1d86dd372ec891bae799f6da20deef16fc199f30080a4ea9de8cef0c619`.
The build script applies `tests/riscv64-linux.config` on top of `tinyconfig` and
sets fixed build identity and timestamp values. It requires host `flex`,
`bison`, and `bc` in addition to the cross toolchain.

## Current machine contract

- one hart and 256 MiB RAM;
- direct kernel entry at `0x80000000` with hart ID in `a0` and DTB in `a1`;
- UART at `0x10000000`;
- CLINT `mtimecmp` at `0x02004000` and `mtime` at `0x0200bff8`;
- test finisher at `0x00100000`;
- M mode grants S/U physical access with PMP, delegates synchronous traps, and
  delegates supervisor external interrupts, and forwards machine timer
  interrupts as supervisor software interrupts.

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

Generic kernel-process creation now allocates an architecture-owned 4 KiB
stack, initializes the saved context to a first-run trampoline, and releases
that stack when the process is reaped. The trampoline enables supervisor
interrupts before calling the kernel task function. Process VMA, ELF entry,
heap, stack, and argument-page addresses use the architecture-selected
`__addr_t`, preserving their 32-bit i386 layout while preventing RV64 address
truncation. This path is compile-gated but is not linked into the bring-up
kernel until generic allocation and address-space activation are available.

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
layouts, and sector I/O before adapting the generic buffer cache.

The generated disk is also a deterministic 8 MiB, 1 KiB-block, revision-0 ext2
filesystem. A bounded read-only gate follows the superblock, group descriptor,
root inode, root directory, `bootstrap` inode, and its direct data block, then
checks the file contents. Fiwix's existing ext2 implementation supports this
same original revision. The staging reader is deliberately small and will be
removed once the virtio device is registered with the generic block layer and
the existing buffer-cache/VFS/ext2 path reaches the same file.

The staging reader resolves direct, single-indirect, and double-indirect file
blocks. `test-riscv64-large-image` pads the executable fixture to 300 KiB, so a
successful handoff proves all three tiers rather than only the first data
block. The disk generator accepts `LINUX_IMAGE=/path/to/Image`; it computes the
metadata layout and free-block counts from the selected image and rejects
inputs that do not fit. The 8 MiB cap is deliberate: it is large enough for a
purpose-built bootstrap kernel but rejects distribution kernels whose size and
feature set would hide the actual handoff requirements.

The smoke scripts run `e2fsck -fn` before QEMU when it is available in `PATH`
or at `/sbin/e2fsck`. This independently checks the generated revision-0
directory format, inode allocation, block accounting, indirect trees, and
bitmap padding rather than relying only on the staging reader under test.

## Generic-kernel compile boundary

The architecture-independent compile audit is available separately from the
bring-up kernel:

```sh
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  test-riscv64-generic-compile
```

It currently compiles 253 C files, including the RV64 process hooks. Eight
explicit architecture boundaries remain excluded: i386 GDT/IDT and boot main,
init, fork, and the three x86 page-table modules for fault, memory, and mmap.
The generic scheduler now calls the tested RV64 callee-saved context switch,
and `ioperm` returns `ENOSYS` because RISC-V has no x86 I/O bitmap.

Shared interrupt save/restore macros map to `sstatus.SIE`, and trap-value,
stack, wait, and U-mode syscall operations use architecture helpers. Internal
allocator addresses use `__addr_t`, which is `unsigned int` on i386 and
`unsigned long` on riscv64. This removes allocator return truncation while
preserving i386 layouts and call widths. The gate still emits 200 pointer-width
warnings, primarily from 32-bit syscall arguments and x86 physical-memory
interfaces; compile success is not yet an LP64 correctness claim.

## Linux Image handoff

The ext2 fixture also contains a position-independent payload with a current
64-byte RISC-V Linux `Image` header. Fiwix loads it at the RV64 2 MiB RAM offset
(`0x80200000`), with a link-time assertion preventing the resident kernel from
growing into that region. The loader checks the advertised offset and size,
little-endian flags, header version 0.2, and both Image magic fields.
The advertised memory size may exceed the file size because it includes
zero-filled sections; the loader bounds that size against the 8 MiB reserved
region and clears the tail before handoff.

The final assembly boundary clears `sie`, `sstatus.SIE`, `sscratch`, and `satp`,
flushes translation and instruction state, then enters with the original hart
ID in `a0` and QEMU DTB physical address in `a1`. The payload independently
checks hart 0, the flattened-device-tree magic, `satp == 0`, and `sie == 0`
before printing its success marker.

The resident M-mode shim implements SBI 0.3 BASE discovery, TIME `set_timer`,
and SRST reset, plus the legacy timer, console, and shutdown calls. The fixture
smoke gate probes BASE/TIME/SRST, programs a timer, observes the delegated
supervisor timer pending bit, and cancels it. The M-mode trap frame preserves
every caller-saved integer register except the SBI return pair.

The optional Linux 6.12.1 gate loads a 1.9 MiB kernel from ext2, then verifies
Linux's own version banner, SBI discovery, clocksource, PLIC, and interrupt-
driven 16550 console initialization. The tiny kernel deliberately has no
userspace, and the gate ends at its expected `No working init found` panic.
This proves a real Linux boot through device initialization while keeping
OpenSBI and a distribution kernel out of the bootstrap runtime. It does not
yet prove a Linux initramfs or userspace handoff.

## Bootstrap compiler findings

The RISC-V TinyCC integrated assembler does not accept all privileged CSR and
fence forms used by the kernel. Those operations live in `ops.S` and are built
with the same GNU assembler needed for the other architecture entry files.
This also keeps compiler memory-order assumptions out of the virtio queue
contract.

The Mes-built `tcc-boot0` miscompiles the constant expression `8UL << 60` as
`0x80000000`. The Sv39 mode field is therefore constructed in assembly while C
passes only the root page-table PPN. The final `tcc-musl` package also reports a
stale `libtcc1.a` path from a removed musl store item; the Makefile derives the
working archive from TinyCC's `install:` directory and allows an explicit
override. Both compiler rungs boot the fixture kernel, and both enter the real
Linux oracle successfully.
