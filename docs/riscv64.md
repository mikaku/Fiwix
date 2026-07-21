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
handles only the callee-saved execution state. The scheduler separately loads
the next process's complete `satp`, flushes stale translations when it changes,
and resets supervisor trap-stack selection before switching registers.

Generic kernel-process creation now allocates an architecture-owned 4 KiB
stack, initializes the saved context to a first-run trampoline, and releases
that stack when the process is reaped. The trampoline enables supervisor
interrupts before calling the kernel task function. Process VMA, ELF entry,
heap, stack, and argument-page addresses use the architecture-selected
`__addr_t`, preserving their 32-bit i386 layout while preventing RV64 address
truncation. This path is compile-gated but is not linked into the bring-up
kernel until generic allocation and page-table construction are available.

The smoke kernel initializes two independent kernel stacks and performs six
cooperative switches before returning to the boot context. This gate checks the
assembly offsets against the C structure and gives later generic-scheduler work
a known-good low-level primitive.

## Sv39 and U-mode design

The bring-up page tables keep a supervisor-only 1 GiB identity mapping for the
kernel and add supervisor-only low leaves for the QEMU UART and test finisher.
Two 4 KiB user leaves map the fixture text read/execute and its stack
read/write. No user mapping is writable and executable at the same time.

An address-space switch gate creates two Sv39 roots that share the kernel and
device mappings but map one supervisor virtual address to different physical
pages. The generic scheduler activation helper selects each root in turn and
the gate verifies the visible marker before restoring the primary root. This
tests full `satp` activation and `sfence.vma` independently of the still-pending
generic page allocator and fault path.

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
bring-up kernel. It also runs a host-side syscall-number translation gate:

```sh
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  test-riscv64-generic-compile
```

It currently compiles 261 C translation units, including the RV64 process,
fork, syscall, trap, signal, and ELF64 exec hooks.
Only the i386 GDT and IDT implementations remain excluded. `kernel/main.c`
retains ownership of common kernel globals and now has an RV64 entry that
installs the generic trap vector before idling; the complete device, memory,
process, and PID 1 initialization sequence is still the next boot milestone.
An architecture CPU implementation reports `riscv64` and the fixed
RV64IMA/Zicsr/Zifencei contract without emulating x86 CPUID, TSC, or port I/O.
The same gate relocatably links those objects with the real RV64 context
switch, generic trap vector, init trampoline, and privileged-operation
assembly. All `riscv64_*` references resolve. An exact 46-symbol allowlist
has therefore fallen to 25 symbols: legacy i386 port I/O, the optional external
IPv4 backend, and three final-linker boundaries. Additions or removals require
an explicit review rather than disappearing in compile-only coverage.
The generic scheduler now calls the tested RV64 callee-saved context switch,
activates the selected process address space first, and `ioperm` returns
`ENOSYS` because RISC-V has no x86 I/O bitmap. Kernel-process creation captures
the active `satp` so a later switch cannot accidentally return to bare mode.

VMA management and page-fault policy no longer inspect `cr3` or x86 page-table
entries directly. Mapping addresses use `__addr_t`, and page release plus
copy-on-write entry updates are owned by `mm/memory.c`. Its RV64 backend now
walks all three Sv39 levels, maps and unmaps 4 KiB user leaves, clones writable
pages as COW, releases empty tables, and builds a 256 MiB identity-mapped kernel
root with supervisor-only finisher, PLIC, UART, and virtio windows. This backend
passes GCC and both bootstrap TinyCC compile gates, but remains compile-gated
until generic init/fork invoke it at runtime.

Generic PID 1 now has an RV64 construction path: it clones the supervisor root
and its low device table, maps separate private RX text and RW stack pages at
the top of the Sv39 user half, allocates a per-process kernel stack, and starts
through an `sret` trampoline. The copied 182-byte assembly stub uses Linux RV64 `openat`, `dup`,
`execve`, and `exit` numbers and builds `argv`/`envp` on its user stack, so it
contains no absolute kernel-address relocations. This path compiles under both
bootstrap TinyCC rungs; runtime syscall dispatch and filesystem integration
remain the next gate.

Fork now delegates page-root and saved-context construction to RV64 code. The
child receives a private Sv39 root, a copied native 272-byte trap frame with
`a0 == 0`, and an architecture return stub that restores every integer register
before `sret`. The generic syscall signature still names the historical i386
`sigcontext`; the RV64 dispatcher will pass its native frame through that
pointer-shaped compatibility boundary until syscall context becomes fully
architecture-neutral.

The generic syscall core accepts architecture-width arguments and a pointer to
an existing saved frame. Its table bound uses the actual pointer element size
and rejects an index equal to the element count. The RV64 translator implements
the compatible stage0 syscall subset: `openat`, `close`, `read`, `write`,
`lseek`, `unlinkat`, `faccessat`, `chdir`, `fchmodat`, `brk`, fork-style
`clone`, `wait4`, `exit`, `getpid`, and `getppid`. Signal translation adds
`kill`, `rt_sigaction`, `rt_sigprocmask`, and `rt_sigreturn`. Unsupported
`*at` directory
descriptors and clone sharing flags are rejected rather than silently given
fork semantics. RV syscall 221 now enters shared `execve` policy, which selects
the architecture ELF64 loader without passing its native trap frame to the
ELF32 implementation. A generic trap vector now dispatches these calls, but the
generic boot replacement must install it before they are exercised by PID 1.

Shared interrupt save/restore macros map to `sstatus.SIE`, and trap-value,
stack, wait, and U-mode syscall operations use architecture helpers. Internal
allocator addresses use `__addr_t`, which is `unsigned int` on i386 and
`unsigned long` on riscv64. This removes allocator return truncation while
preserving i386 layouts and call widths. Physical page conversion also accounts
for QEMU RV64 RAM beginning at `0x80000000`; page-cache and buddy indexes are
relative to that base while kernel pointers remain identity mapped. The gate
still emits 188 pointer-width warnings, primarily from 32-bit syscall arguments
and x86 physical-memory interfaces; compile success is not yet an LP64
correctness claim.

## Generic ELF64 exec design

The first filesystem-backed RV64 loader accepts little-endian static RISC-V
`ET_EXEC` files whose program-header table fits in the first filesystem block.
A pure planning pass validates all load ranges, page-rounded overlap, entry
permissions, mapped program headers, file bounds, and the absence of
`PT_INTERP` before the old image is released. The kernel then creates anonymous
VMAs, eagerly copies every file-backed byte, zeros BSS, and builds a 16-byte
aligned LP64 `argc`/`argv`/`envp`/auxv stack.

Eager population is deliberate: first execution remains deterministic without
requiring a file-backed demand fault before the generic boot path is live. This
initial scope excludes dynamic linking, PIE, and lazy ELF paging. The
loader enforces W^X by dropping write permission from executable segments. The
bootstrap stage0 `hex0-seed` overstates its single segment as RWE, but static
inspection confirms that it writes only to its stack, so it executes correctly
when normalized to RX.

The host-side ELF plan gate mirrors the seed's 392-byte image shape and rejects
wrong class or machine, truncated headers and files, dynamic interpreters,
overlapping or oversized segments, non-executable entry points, and unmapped
program-header metadata. The new ELF, exec, `execve`, and mmap units compile
with both bootstrap TinyCC rungs. A whole-tree `tcc-boot0` audit reaches these
units but still stops later at the pre-existing `net/ipv4.c` return from a
`void` function; only the GCC whole-tree audit is currently claimed.

Three generic exec bugs were fixed while adding this path:

- `do_mmap()` returned `int`, so a successful RV64 stack address near
  `0x4000000000` was truncated into a negative result. Its internal return type
  is now signed `long`, which remains 32-bit on i386.
- Fresh argument-staging pages retained allocator contents. Script rewriting
  could inspect stale bytes, and copying complete pages to userspace could
  disclose kernel allocation data. Both allocation paths now zero those pages.
- A nonzero `PT_PHDR` address was previously enough to satisfy planning. The
  planner now requires the complete program-header table to lie in file-backed
  bytes of a validated load segment.

## Generic trap design

The generic RV64 vector saves the complete 272-byte integer frame used by fork
and exec. `sscratch` exchanges the U-mode stack for the current process's kernel
stack. The entry clears `sscratch` before enabling supervisor interrupts in
bottom-half processing, so a nested timer is classified as a kernel trap rather
than swapping onto the saved user stack. Immediately before `sret`, it restores
the process kernel-stack top to `sscratch` and the user stack from the frame.

User ecalls enter the Linux-number translator and then generic bottom halves.
Supervisor software timer interrupts clear `sip.SSIP`, call the existing timer
IRQ and bottom-half policy through a compatibility context that records whether
the interrupted mode was user or supervisor, and schedule only when returning
to userspace. This matches the i386 rule that nested kernel interrupts do not
preempt at their return boundary.

A firmware-free QEMU gate enters U mode, seeds registers, executes `ecall`, and
checks frame offsets, `sepc` advancement, return-value replacement, user-stack
restoration, and successful `sret`. A separate host policy test covers user and
kernel timer paths, bottom-half accounting, scheduling, syscall failure, page-
fault flag translation, and unsupported causes. Synchronous user exceptions
are translated to `SIGILL`, `SIGTRAP`, `SIGBUS`, or `SIGSEGV`; fault signals
now pass through normal signal disposition instead of making the architecture
trap fatal.

## Generic page-fault design

VMA lookup, protection checks, copy-on-write, demand loading, zero-page setup,
and bounded stack growth now live in an architecture-neutral
`resolve_page_fault()` operation. Architecture trap code supplies the fault
address, read/write and user/kernel classification, whether a valid leaf is
already mapped, and the saved user stack pointer. The resolver returns one of
four typed outcomes: resolved, `SIGSEGV`, `SIGKILL`, or fatal kernel fault. The
i386 wrapper retains its verbose register/VMA diagnostics and signal behavior;
the RV64 wrapper translates instruction, load, and store page-fault causes and
runs bottom halves after a resolved fault.

For a fault raised while the RV64 kernel is accessing userspace, the wrapper
uses the user stack pointer in the process's saved native trap frame rather
than the current supervisor stack. A host policy gate links the real generic
resolver against controlled VMA and page-map fixtures. It covers read
violations, all copy-on-write outcomes, anonymous demand-zero pages, stack
growth, invalid addresses, and kernel demand/fatal paths. Signal outcomes are
queued and consumed by generic signal disposition before scheduling on the
user-return path.

## Generic signal design

Generic `psig()` still owns pending-mask selection, default stop/continue/exit
actions, handler masking, and one-shot disposition. RV64 owns only the native
user-frame transition. It writes the Linux RISC-V real-time signal ABI: a
128-byte `siginfo`, 960-byte `ucontext`, complete 32-register context, and
16-byte-aligned 1088-byte frame. The handler receives the signal number,
`siginfo` pointer, and `ucontext` pointer in `a0` through `a2`.

Every ELF64 image receives a fixed read/execute page immediately below the
Sv39 user limit. Its three-instruction trampoline invokes Linux RV64 syscall
139 (`rt_sigreturn`) and cannot be modified through the non-executable stack.
The initial generic PID 1 trampoline now uses separate RX text and RW stack
pages; `execve` removes both before installing the signal page and final stack.
Signal return validates an executable user PC, restores all integer registers
and the old signal mask, and forces `sstatus.SPP` and `sstatus.SIE` clear before
`sret`.

Host gates exercise generic disposition, masking, frame build/restore, invalid
return PCs, and trampoline bytes. A cross-compiler gate checks the local frame
sizes and offsets against installed Linux RISC-V headers. Alternate signal
stacks and real-time signals above Fiwix's historical 31-signal range remain
unsupported.

This milestone also fixed four shared bugs. The original `SIG_BLOCKABLE`
expression accidentally allowed `SIGSTOP` to be blocked; signal 32 could be
queued but was never scanned; fork cleared only one of its saved signal
contexts; and stack-fault helpers assumed the highest VMA was always
`P_STACK`. The last assumption became observable as soon as the RX signal page
was placed above the stack.

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

The first whole-tree compile audit counted `font-lat9-8x8.c`,
`font-lat9-8x14.c`, and `font-lat9-8x16.c` as standalone units even though the
normal video build includes them textually from `fonts.c`. Compiling and
relocating those objects exposed duplicate font definitions. The manifest now
matches the Makefile and reports 260 real C translation units; this correction
does not remove any kernel code from the audit.
