# RISC-V 64 port

The experimental riscv64 target boots directly on the single-hart QEMU `virt`
machine. It currently proves the firmware-free M-mode entry, the transition to
S mode, fatal trap reporting, machine-timer forwarding, and the architecture
context-switch primitive with two kernel tasks, Sv39 address translation, and a
small U-mode RV64 syscall fixture, polled virtio-mmio block I/O, and an ext2
file lookup. A separate generic image now activates Sv39,
registers UART and virtio block devices, mounts ext2, schedules PID 1, executes
`/sbin/init`, and observes a userspace console write through generic Fiwix
policy. A writable-root gate then execs the unmodified 392-byte stage0-posix
RV64 `hex0-seed` as PID 1 and verifies its decoded output from the disk image.
The nested process-tree gates reproduce the complete phase 1-11 stage0 tool
chain and verify its final M1, hex2, and kaem binaries.
The first two live-bootstrap continuations then complete stage0 phases 12-23
and use those native tools to build the unmodified riscv64
checksum-transcriber and simple-patch manifest entries.
The final chain gate then asks Fiwix to load Linux from that same mutated ext2
root, preserves the original hart ID and DTB contract, mounts the root under
Linux, and executes a static Linux PID 1.

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

# Build a revision-0 ext2 root and execute the real upstream hex0 seed.
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 \
  STAGE0_SEED=/path/to/stage0-posix/bootstrap-seeds/POSIX/riscv64/hex0-seed \
  test-riscv64-stage0

# Reproduce hex0, kaem-0, hex1, and hex2-0 through nested kaem processes.
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 \
  STAGE0_DIR=/path/to/stage0-posix test-riscv64-kaem-phase2

# Continue through catm and the architecture-specific M0 assembler.
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 \
  STAGE0_DIR=/path/to/stage0-posix test-riscv64-kaem-phase3

# Use M0 to reproduce the architecture-specific cc_riscv64 compiler.
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 \
  STAGE0_DIR=/path/to/stage0-posix test-riscv64-kaem-phase4

# Run the uninterrupted phase 1-11 compiler/tool chain (long-running).
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 \
  STAGE0_DIR=/path/to/stage0-posix test-riscv64-kaem-mini

# Complete stage0 and build the first live-bootstrap package (long-running).
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 \
  STAGE0_DIR=/path/to/stage0-posix \
  LIVE_BOOTSTRAP_DIR=/path/to/live-bootstrap \
  test-riscv64-kaem-manifest1

# Replay that boundary and add the second live-bootstrap package.
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 \
  STAGE0_DIR=/path/to/stage0-posix \
  LIVE_BOOTSTRAP_DIR=/path/to/live-bootstrap \
  test-riscv64-kaem-manifest2
```

QEMU must provide its standard `virt` 16550 UART, CLINT, and test finisher.
The test uses `-bios none`; OpenSBI is not part of the runtime contract.

An optional real-Linux oracle is reproducible from a Linux source tree:

```sh
JOBS=8 tests/build-riscv64-linux.sh /path/to/linux-6.12.1 /tmp/linux-build
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 \
  LINUX_IMAGE=/tmp/linux-build/arch/riscv/boot/Image test-riscv64-linux

# Add block, virtio, ext2, and static ELF support for the root handoff gates.
JOBS=8 tests/build-riscv64-linux.sh /path/to/linux-6.12.1 \
  /tmp/linux-root-build tests/riscv64-linux-root.config
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 \
  LINUX_IMAGE=/tmp/linux-root-build/arch/riscv/boot/Image \
  test-riscv64-linux-root

# Run stage0 phases 1-11 and hand the resulting ext2 root to Linux.
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=/path/to/qemu-system-riscv64 \
  STAGE0_DIR=/path/to/stage0-posix \
  LINUX_IMAGE=/tmp/linux-root-build/arch/riscv/boot/Image \
  test-riscv64-kaem-linux
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
truncation. The generic image now exercises the same scheduler switch and
per-process page-table construction for PID 1.

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
bounds, one-page RX load segment, alignment, and entry point before
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
zero from a generated disk fixture. The same test runs twice: once
with QEMU's legacy virtio-mmio v1 transport and once with modern v2. Queue
completion is polled with a finite bound, and the driver verifies both the
virtio request status and exact sector marker before reporting success.

The staging queue remains the transport oracle for discovery, feature
negotiation, DMA addresses, and both queue layouts. The generic image wraps
that transport in a writable major-8 block device; reads and writes pass
through Fiwix's request queue, buffer cache, VFS, and ext2 implementation.
Each test mode receives a private disk copy so mount metadata and stage0 output
cannot make the second transport depend on the first.

The generated disk is also a deterministic 8 MiB, 1 KiB-block, revision-0 ext2
filesystem. A bounded read-only gate follows the superblock, group descriptor,
root inode, root directory, `bootstrap` inode, and its direct data block, then
checks the file contents. Fiwix's existing ext2 implementation supports this
same original revision. The staging reader remains as an independent oracle,
while the generic image mounts the same disk on `/dev/vda`, resolves
`/bootstrap`, and executes the ELF64 file at `/sbin/init`.

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

The real-stage0 gate constructs a revision-0, feature-free ext2 root with host
`mke2fs` and `debugfs`. Its `/sbin/init` is a 64-bit assembly launcher that
calls `execve` with the three arguments required by the unmodified seed:
`/bin/hex0-seed`, `/stage0-input.hex0`, and `/stage0-output`. It does not embed
those paths in kernel policy. After each legacy and modern virtio boot, the host
extracts `/stage0-output` and compares all bytes with the expected decoded
fixture. The accepted seed is exactly 392 bytes; the recorded stage0-posix
artifact has SHA-256
`1b50ceef632b83b79aef0cf91d60bc0cb242a3b2bfba22cb5115d80112b50ac9`.

Two root-fixture bugs were caught by the independent gates. GNU `as` does not
accept a forward section-label difference directly as a `li` operand, so the
launcher pins message lengths and checks them with assembly-time assertions.
Also, `debugfs mknod /dev/console` creates a literal slash-containing name in
the root directory; the builder now enters `/dev` before creating `console`,
and `e2fsck -fn` rejects the malformed form before QEMU starts.
An assembler-only launcher target also triggered an eager top-level `libgcc`
probe through an absent `${CROSS_COMPILE}gcc`; that lookup is now expanded only
by the GCC kernel link that consumes it.

## Generic-kernel compile boundary

The architecture-independent compile audit is available separately from the
bring-up kernel. It also runs a host-side syscall-number translation gate:

```sh
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  test-riscv64-generic-compile
```

It currently compiles 267 C translation units, including the RV64 boot,
process, fork, syscall, trap, signal, and ELF64 exec hooks.
Only the i386 GDT and IDT implementations remain excluded. `kernel/main.c`
retains ownership of common kernel globals and now has an RV64 entry that
installs the generic trap vector, activates the kernel Sv39 root, initializes
the allocator and generic kernel tables, and reserves idle and PID 1.
An architecture CPU implementation reports `riscv64` and the fixed
RV64IMA/Zicsr/Zifencei contract without emulating x86 CPUID, TSC, or port I/O.
The same gate relocatably links those objects with the real RV64 context
switch, generic trap vector, init trampoline, and privileged-operation
assembly. All `riscv64_*` references resolve. The exact unresolved allowlist
has fallen from 46 to nine symbols: six operations referenced only by
compile-audited legacy i386 drivers and three final-linker boundaries.
Additions or removals require an explicit review rather than disappearing in
compile-only coverage. RV64 disables the repository's unimplemented external
IPv4 backend and x86 PCI, BGA, and PS/2 options rather than supplying false
runtime stubs.
The generic scheduler now calls the tested RV64 callee-saved context switch,
activates the selected process address space first, and `ioperm` returns
`ENOSYS` because RISC-V has no x86 I/O bitmap. Kernel-process creation captures
the active `satp` so a later switch cannot accidentally return to bare mode.

## Generic-kernel boot boundary

The generic objects also link into a separate firmware-free image without
changing the default staging kernel:

```sh
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  riscv64-generic-image
make TARGET_ARCH=riscv64 CROSS_COMPILE=riscv64-linux-gnu- \
  QEMU=qemu-system-riscv64 test-riscv64-generic-boot
```

`fiwix-generic` enters through the proven M/S-mode boot assembly, calls the
shared `start_kernel()`, and installs the complete generic trap vector with
interrupts disabled. It then activates the 256 MiB identity-mapped Sv39 root,
initializes memory, process, sleep, buffer, scheduler, inode, and descriptor
state, creates idle and reserves PID 1, and enables interrupts only after timer
bottom halves exist. It registers a polled UART tty and writable virtio block
device, mounts ext2, builds PID 1, and schedules it into U mode. Acceptance
requires three delegated ticks plus a console marker from the filesystem-backed
`/sbin/init`. The final link uses individual function/data
sections and rejects undefined symbols. Weak per-symbol sentinels make legacy
port-I/O and external-network relocations linkable during garbage collection,
then fail the gate if any sentinel survives into the ELF. This proves those
subsystems are absent from the runnable closure rather than silently mapped to
no-op hardware.

The RV64 `kswapd` continuation retains generic memory devices except
`/dev/port`, PTYs, ramdisk, filesystems, and PID 1 policy, but does not register
ATA, floppy, PC serial, or parallel-port devices. Timer policy uses delegated
supervisor ticks without PIT/PIC setup, the absent CMOS RTC reports no procfs
data, keyboard LEDs and console beep have no PS/2/PIT side effects, and reboot
uses SBI SRST. The initial UART transmit and virtio completion paths are polled;
PLIC-backed UART receive and block-completion interrupts remain open.

VMA management and page-fault policy no longer inspect `cr3` or x86 page-table
entries directly. Mapping addresses use `__addr_t`, and page release plus
copy-on-write entry updates are owned by `mm/memory.c`. Its RV64 backend now
walks all three Sv39 levels, maps and unmaps 4 KiB user leaves, clones writable
pages as COW, releases empty tables, and builds a 256 MiB identity-mapped kernel
root with supervisor-only finisher, PLIC, UART, and virtio windows. This backend
passes GCC and both bootstrap TinyCC compile gates. Generic boot now activates
the kernel root and allocator, creates a private PID 1 root, maps U-mode pages,
and switches `satp` at runtime; fork remains policy/compile gated.

Generic PID 1 now has an RV64 construction path: it clones the supervisor root
and its low device table, maps separate private RX text and RW stack pages at
the top of the Sv39 user half, allocates a per-process kernel stack, and starts
through an `sret` trampoline. The copied 182-byte assembly stub uses Linux RV64 `openat`, `dup`,
`execve`, and `exit` numbers and builds `argv`/`envp` on its user stack, so it
contains no absolute kernel-address relocations. The generic image now runs the
stub, opens `/dev/console`, duplicates it to stdout/stderr, executes a static
filesystem ELF64 image, constructs its LP64 initial stack, and observes its
U-mode `write`. Both virtio transport versions pass this gate.

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
the compatible stage0 syscall subset: `dup`, `openat`, `close`, `read`, `write`,
`lseek`, `unlinkat`, `faccessat`, `chdir`, `fchmodat`, `brk`, fork-style
`clone`, `wait4`, `exit`, `getpid`, and `getppid`. Signal translation adds
`kill`, `rt_sigaction`, `rt_sigprocmask`, and `rt_sigreturn`. Unsupported
`*at` directory
descriptors and clone sharing flags are rejected rather than silently given
fork semantics. RV syscall 221 now enters shared `execve` policy, which selects
the architecture ELF64 loader without passing its native trap frame to the
ELF32 implementation. The generic trap vector dispatches these calls for the
filesystem-backed PID 1 path.

Shared interrupt save/restore macros map to `sstatus.SIE`, and trap-value,
stack, wait, and U-mode syscall operations use architecture helpers. Internal
allocator addresses use `__addr_t`, which is `unsigned int` on i386 and
`unsigned long` on riscv64. This removes allocator return truncation while
preserving i386 layouts and call widths. Physical page conversion also accounts
for QEMU RV64 RAM beginning at `0x80000000`; page-cache and buddy indexes are
relative to that base while kernel pointers remain identity mapped. The gate
still emits 158 pointer-width warnings, primarily from 32-bit syscall arguments
and x86 physical-memory interfaces; compile success is not yet an LP64
correctness claim.

## Generic runtime initialization

Supervisor interrupts are disabled before the generic trap vector is installed
and stay disabled through memory, process-table, queue, and timer
initialization. Idle captures the active `satp`, reserved PID 1 receives PID 1,
and only then may delegated software timer interrupts enter generic
`irq_timer()` and bottom-half policy. The boot gate waits for three ticks rather
than accepting successful construction alone.

The QEMU `virt` Goldfish RTC at `0x101000` supplies the initial Unix time. Its
low register latches the matching high half, and the nanosecond value is reduced
to Fiwix's 32-bit seconds representation before the first filesystem operation.
The generic runtime gate rejects an epoch-zero clock.

Fiwix's private `stdarg.h` remains unchanged for i386, but non-i386 targets use
the compiler's ABI-aware `<stdarg.h>`. The old implementation advanced a byte
pointer through a presumed 32-bit stack and cannot represent the RV64 register
argument save area. Both bootstrap TinyCC packages provide their own RISC-V
implementation, and the same `printk` unit compiles under those packages and
cross GCC.

This milestone exposed two startup bugs. Installing the delegated timer vector
while `sstatus.SIE` was still set allowed a pending SSIP to enter generic timer
policy before `current` existed; startup now disables interrupts first and
enables them only after idle and timer bottom halves are ready. The first
`sprintk("%s")` then faulted because the bundled i386-only varargs walker read a
bogus RV64 pointer. The compiler-owned non-i386 varargs branch fixes that fault
without changing the i386 bootstrap path.

The fixed QEMU machine uses `/dev/ttyS0` as `/dev/console` and a writable
major-8 `/dev/vda` root. UART transmit drains generic tty queues by polling the
16550 line-status register. The block adapter reports capacity from the virtio
configuration space and converts each generic block request into bounded 512
byte sector transfers. The generated ext2 fixture includes `/dev/console` and a
header-mapped static `/sbin/init`; `e2fsck -fn` validates the expanded image.

The first scheduled PID 1 reached its ELF64 `write` but returned `EBADF` because
the init trampoline's asm-generic syscall 23 (`dup`) was absent from the RV64
translator. Adding the direct `SYS_dup` mapping and host regression case opens
stdout/stderr correctly and makes the U-mode marker visible.

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
loader preserves the ELF segment permissions. This includes RWE for the legacy
stage0 seeds: `hex1` stores its first-pass label table inside its sole load
segment, so removing `PF_W` from executable segments breaks the canonical
bootstrap binary.

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

The first optional Linux 6.12.1 gate loads a 1.9 MiB kernel from ext2, then
verifies Linux's own version banner, SBI discovery, clocksource, PLIC, and
interrupt-driven 16550 console initialization. That negative-control kernel
deliberately has no userspace and ends at its expected `No working init found`
panic.

The root-capable config overlay adds only block, virtio-mmio block, ext2,
devtmpfs, and static ELF support. Its 2.1 MiB Image mounts the same revision-0
ext2 disk read-only and executes a freestanding assembly PID 1, which prints a
marker and powers down through SBI. This keeps the root-continuity claim
separate from a future distribution userspace or manifest resume claim.

The full generic-kernel route uses Linux's standard reboot kexec command
`0x45584543` as an explicit post-stage0 transition. The ordinary completion
fixture still calls sync and reset, so the established phase 1-11 acceptance
boundary does not silently change. The Linux completion fixture calls sync and
kexec instead. Fiwix disables supervisor interrupts, loads `/linux` through
the already-negotiated polled virtio queue, validates its Image header, clears
`satp`, and enters Linux with the original hart ID and DTB. The saved values
live in initialized data because generic `start_kernel()` clears BSS after the
firmware-free entry code first receives them.

## Bootstrap compiler findings

The complete 267-unit generic kernel can be built with the pinned bootstrap
TinyCC by invoking `riscv64-generic-image-tcc` and supplying `TCC` plus
`TCC_LIBTCC1`. The target passes `--no-warn-mismatch` only at this boundary:
TinyCC currently tags its integer-only RV64 output as double-float ABI even
when passed `-mabi=lp64`, while GNU `as` correctly tags the kernel assembly as
soft-float. Fiwix does not enable or use floating point in either set of
objects. The normal GCC image link remains strict.

Minimal package environments may set `GENERIC_WORKDIR` to an owned build
directory when they do not yet provide `mktemp`. The image script recreates
and removes only that explicit directory. This keeps the source build usable
with commencement's Gash utilities without adding a later coreutils input.
The same recipe sets `GENERIC_MARCH=rv64ima` for bootstrap binutils 2.30,
whose assembler implements the CSR and instruction-fence operations but
predates their later `_zicsr_zifencei` command-line spelling.
The top-level Makefile uses one stable in-tree generated linker-script path.
The previous lazy `$(shell mktemp)` variable was expanded separately on each
recipe line, so the preprocessor wrote one temporary file while the linker
opened a different empty file. The deterministic path is removed after the
link and by `clean`; parsing a TinyCC-only target still does not add `mktemp` to
the package closure.

## Native stage0 process tree

The kaem root builder pins stage0-posix commit
`643598041bf7639883874fe2cdc9d9693c9b03d5` and rejects displaced recursive
submodules. It archives the superproject and every registered submodule into a
revision-0 ext2 image rather than copying generated files from the host working
tree. The short cumulative gates run nested optional/minimal kaem processes and
compare the generated `hex0`, `kaem-0`, `hex1`, `hex2-0`, `catm`, concatenated
M0 input, M0 executable, and `cc_riscv64` intermediates and executable against
canonical hashes after independently reopening each v1 and v2 disk.

The complete `mescc-tools-mini-kaem.kaem` route is a separate long-running
target. Historical tools perform byte-at-a-time file syscalls; keeping that
gate distinct prevents a multi-minute compiler build from weakening the fast
acceptance boundaries. Each short gate includes every preceding phase. This is
deliberate: it identifies the first broken bootstrap contract while the long
gate remains the proof that no host process or pre-generated artifact was
inserted between phases. After the nested kaem process returns, a dedicated
assembly fixture prints the completion marker, invokes Linux RV64 syscall 81
to persist the output filesystem, and invokes syscall 142 with Fiwix's reboot
magic. This exits QEMU through the existing SBI reset path, so the long gate
does not confuse an arbitrary timeout with successful script completion.

The shorter generic PID 1 fixture intentionally remains scheduled after its
userspace marker. Its smoke harness watches the serial log while QEMU runs and
terminates the emulator as soon as the RTC, kernel construction, and userspace
markers are all present. The earlier fixed-duration wrapper always consumed
the full timeout and produced false failures under sustained host contention.
The timeout is now only an upper failure bound; early QEMU exit and incomplete
serial output still fail with the captured log.

The `linux` stage runs that same uninterrupted script and verifies the same
three final hashes before allowing the handoff. The root builder adds the
root-capable Linux Image as `/linux` and a distinct `/sbin/linux-init`; Fiwix's
stage0 launcher remains `/sbin/init`, while the preserved DTB command line
selects `/sbin/linux-init` after handoff. Both legacy and modern virtio paths
must reach a clean Linux poweroff. The final acceptance run also uses the
complete generic kernel compiled by `tcc-musl` and linked by bootstrap
binutils 2.30.

A fresh archived stage0 root does not contain an empty `/sbin` directory. The
first manifest-to-Linux replay therefore failed on the host while installing
`linux-init`, before QEMU started. The Linux-stage builder now creates that
directory before either completion fixture is installed; retained work trees
are not allowed to supply undeclared directory state.

## Live-bootstrap continuation

The manifest-resume gates pin live-bootstrap commit
`9a268c4c39cae952b268bc86da342be2175f03d4`. It first runs stage0's unmodified
`mescc-tools-full-kaem.kaem` and `mescc-tools-extra.kaem`, then verifies the
complete riscv64 answers file before installing the generated tools under
`/usr/bin`. Package inputs are archived from that exact commit, so changes in
the host checkout's working tree cannot enter the root image. The gate runs
live-bootstrap's unmodified
`checksum-transcriber-1.0/pass1.kaem` and requires the resulting executable to
match its canonical riscv64 SHA-256,
`1c3021d8051fefd615edb50907e3015d810f974b5b9461f8f9aa383478620a0d`, on
both legacy and modern virtio transports. The second gate deliberately replays
that complete boundary before running the unmodified
`simple-patch-1.0/pass1.kaem`; its `/usr/bin/simple-patch` must match upstream's
canonical riscv64 SHA-256,
`dc72b76c8835b1a08b1ecaa2ab8e9179c290805dd2c8bf3636004f375948c238`.
Keeping the manifests cumulative proves the package ordering and prevents a
host-produced first package from becoming an undeclared input to the second.

The launcher deliberately has two scripts. The seed script is restricted to
the minimal kaem command language through phase 11; its final command starts
the newly generated full kaem. Only that continuation assigns environment
variables, changes directories, and drives phases 12-23 and live-bootstrap.
The first attempt put those assignments in the minimal script, which treated
`ARCH=riscv64` as a program and aborted. An earlier acceptance wrapper also
assumed phase 11 had produced `mkdir`, `cp`, and the rest of the installation
tool set. Those utilities are outputs of the later stage0 phases, so the
manifest gate now completes and checks the entire stage0 answers set rather
than injecting host-built substitutes.

The first installation attempt then reached all canonical stage0 answers but
failed when native `cp` called `getcwd()` while examining an absolute
destination. Linux RV64 assigns `getcwd` syscall 17, which was missing from
the architecture translator even though generic Fiwix already implements the
same buffer, size, and return-value contract as `sys_getcwd`. After that call
was mapped, the generated `mkdir` exposed the same omission for `mkdirat`
syscall 34. The utility did not propagate `ENOSYS`, so `cp` later received
`ENOENT` while creating `/usr/bin/blood-elf`; its historical null `FILE *`
check used `fdest < 0` and the process faulted in `fputc`. Mapping `AT_FDCWD`
`mkdirat` directly to generic `sys_mkdir` fixes the original directory-creation
failure. Host unit gates preserve full-width pointers and modes and reject
unsupported directory descriptors. These are kernel ABI fixes; the generated
utilities and upstream package recipe remain unchanged.

The first complete package invocation also inherited stage0's relative
`TMPDIR=../riscv64/artifact`. That path is valid while building from the source
root but not after entering the live-bootstrap package directory, so
`M2-Mesoplanet` rejected it before compilation. The continuation now resets
`PATH`, `M2LIBC_PATH`, and `TMPDIR` to absolute installed-root paths at the
manifest boundary. This keeps later package behavior independent of the
stage0 driver's working directory.

The successful package build initially failed only its post-run filesystem
check. Four compiler temporaries had been unlinked correctly: their directory
entries, data blocks, and inode bitmap bits were all released. Their deletion
times were nevertheless near January 1970 because the initial riscv64 port set
the wall clock to zero, while the host-created ext2 image was dated later.
`e2fsck` classified those stale times as remnants of a corrupt orphan list. The
QEMU `virt` Goldfish RTC initialization fixes the filesystem metadata at its
source; weakening the integrity gate or repairing the image on the host would
have hidden a kernel timekeeping defect.

The continuation plan advances one upstream manifest entry at a time. The
next boundary is Mes 0.27; later boundaries bring up TinyCC
before reconnecting to the already-proven Fiwix-to-Linux root handoff. Each
boundary will retain the pinned source revision, native output hash, dual
virtio boot coverage, and a distinct completion marker so failures identify
the first unsupported package contract.

The first process-tree run exposed two ABI mistakes. The clone translator
required parent-TID, TLS, and child-TID registers to be zero even when flags 17
(`SIGCHLD` only) make Linux ignore those arguments; the hand-written kaem seed
leaves them unspecified. The translator now validates only the flags and null
child stack that define fork semantics. The ELF planner also normalized RWE
segments to RX. That happened to work for `hex0-seed`, but `hex1` writes its
first-pass label table inside its sole load segment and faulted at
`0x600045c`. Filesystem exec now preserves the permissions declared by each
static ELF segment.

The first phase-3 run exposed an LP64 hash-table overrun. The buffer, inode,
and page caches stored arrays of pointers, but their allocation or bucket-count
calculations used `sizeof(unsigned int)`. On RV64, the page cache consequently
addressed twice the available buckets and overwrote the adjacent page table.
`catm` made the corruption deterministic when a 65 KiB read faulted in another
lazy heap page. All three tables now derive allocation, bucket, and diagnostic
counts from `sizeof(*hash_table)`; keeping that calculation tied to the declared
element type preserves i386 behavior and prevents another pointer-width
assumption from diverging.

The completion fixture also exposed signed-argument conversion hidden by the
i386 dispatcher. Generic syscall dispatch uses machine-word arguments even
when a handler, such as `sys_reboot`, declares 32-bit `int` parameters. The
RV64 syscall adapter now explicitly sign-extends reboot's three integer
arguments before generic dispatch; otherwise the high-bit reboot magic reaches
the handler zero-extended and is rejected. The completion path also maps the
standard RV64 sync syscall before reset. Without it, an immediate SBI reset can
discard dirty ext2 metadata even though every stage0 process completed.

Constructing the complete source root found a host-side test bug as well:
`git archive` emits a gitlink but no submodule contents. The root builder now
checks `git submodule status --recursive` and archives each pinned submodule at
its registered path before creating ext2.

The Linux continuation exposed a second ext2-reader assumption. The custom
fixture builder allocated every Image block, but `mke2fs -d` preserves holes
in the real Linux Image. The staging loader treated a zero data-block pointer
as an I/O failure. It now distinguishes failed indirect-block reads from valid
sparse mappings and explicitly zero-fills holes before validating and entering
the kernel.

The 268 compiled translation units are recorded in
`tests/riscv64-generic-sources.list` rather than discovered with
`find -name`. Commencement's Gash `find` lacks that predicate, and an exact
manifest also makes additions to the reviewed kernel closure fail the expected
count until they are deliberately classified. Object names use the manifest
index, avoiding one emulated Gash/Guile `tr` process per source file.

TinyCC also accepts but does not implement `-ffunction-sections`. Its image
therefore retains dormant `inport_b` and `outport_b` references from mixed
legacy/generic translation units. The TinyCC target permits exactly those two
weak dead stubs through `riscv64-generic-tcc-stubs.expected`; the image build
fails if the retained set changes. Neither stub is reachable from the RV64
startup path, and the resulting image must still pass the v1 and v2 virtio
userspace boot gates.

The first full TinyCC image also exposed a 64-bit constant-folding defect:
TinyCC compiled `(1UL << 63)` as bit 31, causing a supervisor software
interrupt to enter the fatal path. Trap dispatch now tests the sign of the
64-bit `scause` value and masks the low cause code explicitly. This avoids
constructing the high interrupt bit and preserves the same RISC-V policy.

TinyCC similarly zero-extended the constant-folded RV64 expression
`~((unsigned long)4096 - 1)`, reducing `PAGE_MASK` to 32 bits and turning the
initial high user-stack mapping into a multi-billion-page loop. It does
materialize the equivalent `0xFFFFFFFFFFFFF000UL` correctly, so RV64 defines
that width-explicit mask while i386 retains the original expression. The same
rule is applied to RV64's 16-byte stack masks, ELF page masks, and `sstatus`
SPP-clear mask; all were audited after the defect was found.

The later pinned TinyCC also lowers some aggregate initialization in
`printk.c` to a freestanding `memset` call. Fiwix now exports conventional
`memset` and `memcpy` compiler entry points as thin wrappers around its existing
`memset_b` and `memcpy_b` implementations, so compiler lowering does not depend
on a hosted libc.

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
matched the Makefile at 260 real C translation units; subsequent boot, CPU,
UART, virtio, ext2 handoff, and Linux Image additions raise the current count
to 267 without
reintroducing the textual includes.
