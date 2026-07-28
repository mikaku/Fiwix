# ARMv7 port

## Scope and bootstrap boundary

Fiwix targets ARMv7 AArch32 on an ARMv8-capable machine. The existing native
bootstrap begins from the canonical AArch64 stage0 seed, uses AArch64
M2-Planet to emit ARMv7, and enters AArch32 before GNU Mes. Fiwix belongs
after that pivot: its userspace ABI, ELF format, compiler inputs, and output
hashes are all ARMv7.

This branch is stacked on the RISC-V port while its generic Fiwix interfaces
remain under review. Once that PR merges, the ARM commits will be rebased onto
upstream `master`. ARM-specific code must stay under `arch/arm` or behind
`CONFIG_ARCH_ARM`; it must not silently change i386 or RISC-V behavior.

The reference platform is QEMU `virt` with a Cortex-A15:

- 128 MiB or more RAM beginning at `0x40000000`;
- PL011 UART at `0x09000000`;
- GICv2 distributor/CPU interfaces at `0x08000000`/`0x08010000`;
- virtio-mmio transports in `0x0a000000..0x0a004000`; and
- an FDT pointer supplied in `r2` by QEMU's direct kernel handoff.

## Milestones and acceptance gates

1. **Boot:** normalize HYP entry to SVC, establish a stack, preserve the DTB
   pointer, print through PL011, and stop through PSCI.
2. **Privilege and traps:** install high and low vectors, initialize GICv2 and
   the architectural timer, enter USR mode, and return through SVC/abort/IRQ
   paths with a complete register frame.
3. **Process ABI:** translate Linux ARM EABI syscalls, load static ELF32
   executables, implement short-descriptor page tables, fork/exec/wait, page
   faults, and signal frames.
4. **Storage and PID 1:** discover modern and legacy virtio block transports,
   mount writable ext2, and run an ARMv7 `/sbin/init`.
5. **Bootstrap:** run the ARMv7 post-pivot Mes/TinyCC chain, compare exact
   artifacts with the independent user-mode route, and test both virtio modes.
6. **Continuation:** hand the same root to a pinned Linux ARM Image and
   revalidate selected generated tools as PID 1.

Each gate must have a deterministic marker and a host-side negative check.
Booting a custom fixture is not evidence that the process ABI or bootstrap
chain is complete.

## Build and current status

With a GNU ARM cross compiler:

```sh
make TARGET_ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- clean
make TARGET_ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- test-arm
```

Clang can be used for the first freestanding gate:

```sh
make TARGET_ARCH=arm CCEXE=clang \
  ARM_CC_TARGET=--target=arm-linux-gnueabihf \
  CROSS_COMPILE=arm-linux-gnueabihf- LIBGCC= test-arm
```

The generic image scripts default an unset `GENERIC_CC_TARGET` to Clang's
`--target=arm-linux-gnueabihf`. An explicitly empty value selects a native ARM
compiler driver, including a compiler produced by the bootstrap chain.
`GENERIC_FLOAT_ABI` similarly defaults to `soft`; the ARM TinyCC target leaves
it empty because bootstrap TinyCC selects its own softfp code-generation mode
and rejects the GCC option spelling:

```sh
make TARGET_ARCH=arm CROSS_COMPILE=/path/to/bootstrap-binutils/bin/ \
  TCC=/path/to/arm-tcc-musl \
  TCC_INCLUDE=/path/to/tinycc/include \
  TCC_LIBTCC1=/path/to/libtcc1.a \
  QEMU_ARM=qemu-system-arm test-arm-generic-tcc
```

The first post-pivot M2 gate uses the same TinyCC-built kernel:

```sh
make TARGET_ARCH=arm CROSS_COMPILE=/path/to/bootstrap-binutils/bin/ \
  TCC=/path/to/arm-tcc-musl \
  TCC_INCLUDE=/path/to/tinycc/include \
  TCC_LIBTCC1=/path/to/libtcc1.a \
  STAGE0_DIR=/path/to/completed/arm-pivot \
  QEMU_ARM=qemu-system-arm test-arm-m2-pivot
```

The next boundary executes the independently built ARMv7 Mes:

```sh
make TARGET_ARCH=arm CROSS_COMPILE=/path/to/bootstrap-binutils/bin/ \
  TCC=/path/to/arm-tcc-musl \
  TCC_INCLUDE=/path/to/tinycc/include \
  TCC_LIBTCC1=/path/to/libtcc1.a \
  ARM_MES=/path/to/arm-pivot/mes-0.27.1/bin/mes-m2 \
  QEMU_ARM=qemu-system-arm test-arm-mes
```

Milestones 1 through 4 and the first boundary of milestone 5 are implemented
on this branch. Milestone 3 links the complete generic kernel and boots through
physical-memory, process-table, and idle-address-space initialization under
QEMU. Both virtio transport versions mount and persist writes through the
generic ext2 stack, and filesystem-backed PID 1 completes its signal, fork,
copy-on-write, and wait path. The ARM VM
layer owns 16 KiB ARMv7
short-descriptor roots, seeds supervisor-only RAM/device mappings, validates
user section mappings, derives TTBR0, activates a process root with a complete
TLB flush, installs 1 KiB coarse tables with 4 KiB user leaves, and clones
roots independently for controlled VM gates. The boot oracle switches between
roots at `0x47e00000` and `0x47e04000`; each maps
virtual `0x00300000` to a different physical section, and the QEMU gate checks
both values before restoring the primary root. It copies a position-independent
ARM EABI fixture to `0x47000000`, maps the ELF header and program-header table
at user virtual `0x00100000`, and enters code at `0x00101000`. A separate
execute-never user stack maps `0x47100000` at
`0x00200000..0x002fffff`. A coarse table at `0x47e08000` maps one writable,
execute-never user page at `0x00400000`; user mode verifies its initial value,
writes a new sentinel, and the exit path verifies the physical page changed.

The process fixture is a standalone static ELF32/ARM `ET_EXEC` embedded in the
bring-up image. A pure bounded planner validates its ELF identity, machine,
type, header sizes, program-header table, entry point, source ranges, user
virtual ranges, alignment, non-overlap, mapped program-header metadata, absence
of `PT_INTERP`, and `p_filesz <= p_memsz`. The freestanding loader consumes
that plan, copies every `PT_LOAD` segment, and clears BSS before enabling
translation. The generic `execve` path now consumes the same plan, reads
segments across filesystem blocks, eagerly allocates each mapped page, clears
BSS, constructs an AArch32 `argc`/`argv`/`envp`/auxv stack, and replaces the
native saved user frame. It also maps a read/execute `rt_sigreturn` trampoline
at `0x3ffff000`, leaving the argument stack below it. A host runtime gate
exercises the real loader against a multi-block fake inode and verifies the
copied image, zero fill, VMAs, heap, stack pointers, auxiliary vector, entry
state, signal mapping, and instruction-cache invalidation.

The ARM EABI-to-generic syscall policy is host-gated and included in the
generic image. Legacy ARM numbers for the bootstrap file/process calls match
Fiwix's i386-indexed table, but the translator still dispatches them explicitly
so unsupported entries do not become accidental ABI. It covers
`exit`/`exit_group`, fork-compatible `clone`,
read/write/open/openat/close, exec, wait4, cwd and basic pathname operations,
descriptor duplication, brk, process IDs, kill, sync, and reboot. The `*at`
subset currently accepts only `AT_FDCWD`; `unlinkat` rejects flags, and
`clone` accepts only `SIGCHLD` with a null child stack.

The ARM `arch_context` now records callee-saved registers, the scheduler
continuation, TTBR0, and the privileged stack instead of falling through to
the i386 TSS layout. The AArch32 switch primitive saves and restores r4-r11,
SP, and LR; TTBR0 activation remains a separate ordered operation for the
scheduler. Its QEMU gate runs an alternate continuation twice on an independent
stack and verifies every callee-saved sentinel after resumption. Architecture
helpers now provide IRQ masking/state restoration, WFI, DFAR/SP access, unified
TLB invalidation, and an EABI three-argument user SVC without selecting x86
inline assembly. Kernel, first-user-entry, and copied-fork-frame continuations
have ARM setup hooks and fixed assembly entry points. A generic
ownership layer reserves one aligned root for each of Fiwix's 64 process slots,
associates every allocated root with its `struct proc`, rejects forged
release/activation requests, creates a clean user half for the future fork
path, and deterministically reuses released slots. It is host-gated but is not
linked into the freestanding oracle. Generic process initialization now
initializes the pool; kernel-task creation, zombie release, scheduling, and
fork setup use explicit ARM hooks instead of the i386 fallback. Each process
owns an independently mutable root and subordinate coarse tables.

The generic ARM memory backend now allocates coarse tables, maps and unmaps
4 KiB user pages, reports generic mapping permissions, clones private pages
read-only for fork, performs refcounted copy-on-write, and reclaims empty
tables. A host runtime gate executes the real walker against a fixed
low-address RAM window and verifies parent/child data isolation and page
counts. The shared initializer reserves and activates a 16 KiB-aligned kernel
root, and process teardown releases subordinate tables before returning its
fixed root. The generic boot gate now executes that initializer and activates
the owned idle root in the linked 271-unit kernel.

Generic PID 1 construction now has an ARM path as well. It creates a clean
process root, maps a private read/execute trampoline at `0x3ffff000` and a
read/write stack below it, allocates a privileged stack, and enters user mode
through the ARM continuation. The copied 168-byte position-independent
trampoline opens `/dev/console`, duplicates stdin to stdout/stderr, builds
32-bit `argv` and `envp` vectors on its aligned stack, and calls ARM EABI
`execve("/sbin/init", ...)`. Its object has no relocations. This path is
cross-compiled and shape-gated, and the filesystem-backed ARM ELF32 loader now
provides the generic runtime transition from that trampoline to a static ARM
executable.

The fixture preserves a complete register frame across SVC, alignment abort,
section-permission abort, and IRQ. It proves that USR mode cannot read the
identity-mapped kernel at `0x40010000`; both abort handlers and the timer IRQ
signal completion by modifying the saved user frame. Generic process lifecycle
files and private physical-page operations now cross-compile for ARM, have
host runtime gates, and are linked into the ARM kernel. Device-backed
filesystem access and bootstrap execution remain incomplete.

The first ARM signal boundary is now host-gated as well. It uses the Linux
AArch32 84-byte `sigcontext`, 744-byte `ucontext`, and 888-byte real-time
signal frame, with their sizes and offsets independently cross-checked against
the target Linux userspace headers. Delivery grows the writable stack,
validates an A32 executable handler, saves all integer/user registers and the
blocked mask, and returns through syscall 173 on the fixed executable
trampoline. ARM EABI syscalls 174 and 175 translate Linux `rt_sigaction` and
`rt_sigprocmask`; return validates the restored PC, SP, CPSR mode, and mask.
This code cross-compiles with the common signal unit and is included in the
linked generic kernel. The corresponding generic vector entry is live-gated
separately: a user SVC
transfers a complete 72-byte frame onto the process's banked SVC stack, calls
the architecture dispatch boundary, restores every register and user-bank
SP/LR, and resumes at the following A32 instruction under QEMU 7.2 and 8.2.

The generic C trap policy now routes user SVC through the explicit EABI
translator, classifies ARMv7 short-descriptor prefetch and data aborts, and
passes translation, access-flag, and permission faults through the common
page-fault resolver. It distinguishes write faults for copy-on-write, reports
alignment aborts as `SIGBUS`, delivers other user exceptions as `SIGILL` or
`SIGSEGV`, and recovers the saved user SP when a kernel access faults on
behalf of a process. Timer IRQ dispatch preserves the raw GIC acknowledge
token, rearms the level-triggered physical timer before EOI, and invokes the
common timer and bottom-half paths with the correct privilege marker. The host
gate covers user and kernel paths, signal delivery, scheduling order,
unknown/spurious IDs, and IRQ acknowledge ordering; target compilation covers
the CP15 fault-register readers. The linked image provides the concrete
GIC/timer access hooks. Generic startup now enables the GICv2 distributor and
CPU interface, unmasks physical timer PPI 30, and waits for three interrupts
from the architectural timer before accepting the runtime.

PL011 is registered as `ttyS0` at major 4, minor 64 and as the system-console
device at major 5. Output remains polled so the first complete kernel does not
depend on a UART receive or transmit interrupt path. The boot gate requires
the registered character device and TTY, a `printk` marker emitted through the
TTY queue, the live firmware-DTB marker, three timer ticks, and the final
direct-console marker before PSCI shutdown.

The DTB parser validates the bounded structure and strings blocks, discovers
RAM from root-level memory nodes, caps it at the current 128 MiB short-
descriptor contract, and records at most 32 root-level `virtio,mmio` regions.
Generic startup retains the parsed platform data before enabling its new
translation root. When the complete DTB lies inside managed RAM, every page
overlapping that blob is reserved before it can enter the allocator. A DTB
above the current 128 MiB managed-memory cap needs no allocator reservation,
but its already-parsed platform data remains available. Host gates cover QEMU
`virt` DTBs for 64, 128, and 256 MiB. Complete-kernel boots at both 128 and
256 MiB require live DTB discovery, at least one virtio transport, and
reservation whenever the blob overlaps managed RAM.

The ARM block path probes only those retained DTB regions and selects the
first virtio block device. It supports both version-1 legacy virtio-mmio,
using the guest page size and queue PFN registers, and version-2 modern
virtio-mmio, negotiating `VIRTIO_F_VERSION_1` and programming explicit
descriptor, available, and used-ring addresses. One eight-entry queue occupies
two 4 KiB-aligned pages so the legacy used-ring alignment and modern split-ring
layout share the same storage. Requests use a three-descriptor header, data,
and status chain. The driver polls completion and acknowledges any resulting
transport interrupt; GIC routing is deliberately unnecessary at this
bootstrap milestone. ARM `dmb sy` barriers order descriptor publication,
notification, and completion even though the current MMU contract leaves the
data cache disabled.

The transport is exposed through Fiwix's block API as writable major 8,
minor 0 `/dev/vda`. Startup initializes the generic buffer, inode, and file
descriptor tables, mounts an ext2 root read-write, reads `/bootstrap` through
VFS, replaces its fixed-size initialization marker, and flushes superblock,
inode, and buffer state. The acceptance check then reads the file's physical
sector directly through virtio and compares the replacement marker, bypassing
the buffer and page caches. A host-built deterministic revision-0 ext2 fixture
keeps this gate independent of host `mkfs` defaults. The smoke test runs
legacy and modern transports at both 128 and 256 MiB and requires `e2fsck -fn`
to accept every modified image.

The same deterministic root contains a static `/sbin/init` and
character-device `/dev/console`. The init ELF has one read/execute load
segment, no interpreter or relocations, and uses only ARM EABI syscalls. It
checks its eight-byte-aligned `argc`/`argv` stack, installs and returns from a
`SIGUSR1` handler, forks, writes the child's copy-on-write stack, exits the
child with status 42, and verifies that value through `wait4`. Distinct console
markers prove signal delivery, child execution, parent reaping, and final PID
1 completion before the root process invokes the ARM reboot path. This live
gate exercises the same generic loader, scheduler, abort, signal, and process
teardown code intended for bootstrap userspace.

The first bootstrap workload runs from a deterministic ext2 root under both
legacy and modern virtio. PID 1 forks, the child executes the canonical
ARMv7 M2-Planet at virtual address `0x00010000`, and the parent waits for a
zero status before syncing and rebooting. M2-Planet input SHA-256 is
`a5b4d5e77906b18079203061f06fabb21ec06e5d6a5bfe8d363dc1b395ddf797`.
It compiles the same fixed C source and M2libc inputs used by the independent
builder-hex0 route. The resulting 376,490-byte M1 has SHA-256
`9c3a8e2878c673b074a51157704fd84c8f92f96b0506c93a390e469b9f8cc543`.
The host extracts that file from each modified ext2 image, checks its size and
hash, and runs the filesystem integrity gate.

The next filesystem-backed process tree pins the 357,531-byte ARMv7 `mes-m2`
at SHA-256
`8aa74fb3cecbcf4bb7bea9f9e7764f4f6549227cf68501da14d6a83302eb068c`.
The deterministic root includes Mes's `mes/module` and `module` trees, and PID
1 supplies an absolute `GUILE_LOAD_PATH` rather than depending on a host build
path. The child loads its Scheme boot files from ext2, evaluates a fixed
expression, emits the acceptance marker from Mes itself, and exits zero. The
parent wait/sync/reboot path and both virtio versions use the same gate as the
M2 boundary.

The Clang ELF32 process oracle is 20,484 bytes with SHA-256
`908d9f271ec3358d2a47f7f34b3439665af0dac3a940c1ccab115b762a0966f6`.

The complete generic compile, link, and boot gate is:

```sh
make TARGET_ARCH=arm CCEXE=clang \
  ARM_CC_TARGET=--target=arm-linux-gnueabihf \
  CROSS_COMPILE=arm-linux-gnueabihf- \
  GENERIC_RUNTIME=/path/to/soft-float/libgcc.a \
  test-arm-generic-boot
```

`GENERIC_RUNTIME` must use the same soft-float EABI as the kernel. The host
`arm-linux-gnueabihf` runtime is not compatible even though its binutils can
assemble and link the soft-float objects.

## Remaining implementation sequence

1. Drive MesCC from the filesystem-backed Mes process, then continue through
   the post-pivot TinyCC manifests while comparing each independently produced
   artifact.
2. Add the same-root Linux continuation and revalidate selected generated
   tools as PID 1.

## Design and bug log

- Canonical ARMv7 M2-Planet links its single load segment at `0x00010000`.
  The first ELF policy inherited the bring-up fixture's `0x00100000` floor and
  rejected the real bootstrap binary. Lowering the leaf floor exposed a second
  bug: the page walker aligns `0x00010000` down to L1 slot 0, but coarse-table
  attachment incorrectly applied the leaf floor to that table's `0x00000000`
  container address and returned `ENOMEM`. The VM policy now distinguishes an
  L1 table that spans valid user pages from the leaf pages it contains: slot 0
  may hold a coarse table, leaves below `0x00010000` remain forbidden, and a
  host gate checks both sides of the boundary.
- The first Mes root contained only `mes-m2`, so the interpreter started but
  failed with `no such file: boot-5.scm`. Mes's executable is not a standalone
  Scheme runtime: its pinned `mes/module` and `module` trees are runtime inputs.
  The root builder now installs both trees and PID 1 supplies
  `GUILE_LOAD_PATH=/mes/module:/module`; the acceptance marker must come from
  the evaluated Scheme expression.
- The first ARM source additions used `GPL-2.0-or-later` SPDX tags copied from
  an unrelated convention even though Fiwix uses its own project license.
  Every ARM source, public header, and linker-script addition now carries the
  same Fiwix License notice used by the repository's architecture code.
- Generic abort handling uses the architectural DFSR/IFSR status rather than
  treating every abort as a page fault. Translation, access-flag, and
  permission statuses enter `resolve_page_fault()`; alignment maps to
  `SIGBUS`, and unsupported kernel aborts stay fatal. The saved PC is left on
  the faulting instruction so a resolved fault retries it.
- GIC EOI requires the complete IAR token, not just its 10-bit interrupt ID.
  The policy keeps both values, treats IDs 1020 through 1023 as spurious,
  completes unknown claimed interrupts before rejecting them, and requires
  the physical timer to be rearmed before EOI so its level is deasserted.
- A kernel IRQ originally restored its exception PC through SVC `lr` without
  preserving the link register of the interrupted SVC code. An IRQ taken in
  `arm_wait_for_interrupt()` therefore returned to that helper's final
  `bx lr` forever even though the timer continued to increment
  `kstat.ticks`. Privileged frames now preserve the interrupted SVC SP/LR and
  use `RFE` to restore PC/CPSR without losing the caller's link register; user
  frames retain the existing banked USR SP/LR path.
- `printk` used a plain `char` for its `-1` log-level sentinel. ARM EABI makes
  plain `char` unsigned, so the sentinel became 255, default-level selection
  never ran, and every system-console character was filtered out even though
  the PL011 callback ran. Both persistent log-level states now use `int`; the
  generic smoke gate requires a complete `printk` line through the PL011 TTY.
- QEMU ARM `virt` describes addresses and sizes with two 32-bit FDT cells even
  though Fiwix is AArch32. Combining those cells in `unsigned long` would
  discard the high half and shift by the type width. The parser uses an
  explicit 64-bit accumulator, then accepts only regions representable in the
  32-bit kernel address space.
- ARM `virt` exposes 32 virtio-mmio windows spaced 0x200 bytes apart. The
  RISC-V port's fixed 0x1000-stride probe is therefore not portable. ARM keeps
  a bounded list of root-level `virtio,mmio` `reg` tuples from the firmware
  DTB; the block driver probes only that discovered list and rejects windows
  too small to contain the block-capacity configuration.
- Fiwix block numbers are signed 32-bit values while virtio capacity and sector
  numbers are 64-bit. The adapter rejects negative blocks and performs the
  multiplication and end-of-device check in an explicit 64-bit type, avoiding
  both signed wraparound and a truncated modern configuration value.
- A first adapter compile relied on another architecture's incidental
  `NULL` definition. The ARM block unit now includes the Fiwix string header
  explicitly, keeping each source's freestanding dependencies self-contained.
- The original generic smoke script redirected QEMU to a temporary log and
  used bare positive `grep` commands under `set -e`. A missing emulator or
  marker exited before printing that log, then the cleanup trap deleted the
  evidence. The gate now validates the emulator first and prints the complete
  captured boot on every QEMU or marker failure.
- Expanding the ext2 fixture from one directory to root plus `/sbin` and
  `/dev` initially left the block-group `used_dirs_count` at one. `e2fsck`
  counted three and rejected the image; the deterministic generator now
  updates that metadata together with free blocks and inodes.
- ARM's `adr` pseudo-instruction can encode only offsets representable by one
  data-processing immediate. Two init marker addresses crossed that range and
  failed assembly. The fixed-address `ET_EXEC` now uses linker-resolved literal
  loads for fixture symbols; the separately copied init trampoline remains
  relocation-free and position-independent.
- A read-back through `bread()` after the ext2 write would only prove that the
  dirty buffer cache contained new bytes. The writable gate flushes all
  filesystem state and uses a direct 512-byte transport request against the
  mapped ext2 data block, so a stale or non-writing block adapter cannot pass.
- Assigning the 264-byte parsed-DTB result into persistent boot state caused
  Clang to emit an undefined freestanding `memcpy`. The copy now names each
  scalar field explicitly, preserving the generic image's no-undefined-symbol
  link contract.
- ARM has two relevant execution states in this bootstrap. Fiwix deliberately
  targets ARMv7 after the existing AArch64-to-AArch32 compiler pivot rather
  than introducing an unproven ARMv7 seed or a new AArch64 Mes backend.
- QEMU may enter a 32-bit kernel in HYP mode when virtualization is enabled.
  The entry code preserves `r2`, programs `ELR_hyp`/`SPSR_hyp`, and enters
  masked SVC mode before touching C state.
- The first HYP return wrote `SPSR_hyp` with the banked-register form while
  already executing in HYP. Both QEMU 7.2 and 8.2 treated the following `eret`
  as an illegal exception return and vectored to address `0x4`, before the
  console was initialized. ARM's ordinary `spsr_cxsf` write selects the
  current HYP SPSR and makes the same image complete the virtualization-on
  smoke gate.
- The kernel is linked at `0x40010000` and objcopied to a raw image for QEMU's
  ARM boot protocol. Supplying the ELF directly jumps to its entry but leaves
  `r2` zero; the raw `-kernel` path loads the same bytes at `0x40010000` and
  passes the generated FDT in `r2`.
- System-console output uses PL011 polling. Interrupt-driven UART I/O remains
  outside the bootstrap boundary; GICv2 is used for the architectural timer,
  whose PPI and acknowledge ordering are independently gated.
- The initial Clang build passed `--target=arm-linux-gnueabihf` through the
  shared `ARCH` variable into GNU `as`, which interpreted it as malformed
  `--target-help`. Compiler target selection is now separate from ISA flags,
  leaving existing assembler command lines unchanged.
- GNU ARM `as` also rejects the compiler-only `-marm` switch. ARM-state
  selection is kept in the compiler `CPU` flags; assembly sources declare
  `.arm` explicitly.
- ARMv7-A alone does not promise the virtualization extension, so GNU `as`
  rejects `ELR_hyp`, `SPSR_hyp`, and `eret` unless the source declares
  `.arch_extension virt`. The reference Cortex-A15 has that extension and the
  entry source now states the requirement explicitly.
- QEMU advertises PSCI `method = "smc"` for this machine even with HYP
  virtualization enabled. An HVC `SYSTEM_OFF` returned to the kernel and left
  the smoke test spinning; the entry source declares the security extension
  and uses the advertised SMC conduit.
- HYP entry must grant SVC mode access to the physical counter and timer in
  `CNTHCTL`; programming `CNTP_TVAL` at PL1 is otherwise trapped back to HYP
  before the kernel's vector table can report a useful failure.
- The timer DTB identifies the non-secure physical PPI as GIC interrupt 30.
  The gate enables that banked PPI explicitly and rejects every unexpected
  interrupt ID.
- A level-triggered timer must be disabled before GIC EOI. Reusing the timer
  arm helper with a zero interval re-enabled an already-expired deadline and
  produced an IRQ storm that prevented USR mode from observing the completion
  flag.
- Exception assembly saves r0-r12, exception PC/CPSR, and the banked USR
  r13/r14. The USR fixture keeps independent sentinels live across SVC, a
  recoverable alignment data abort, and IRQ so a frame-layout or restore
  regression fails before the success marker. The abort gate validates both
  `DFSR` and `DFAR` before skipping the faulting instruction.
- The original negative smoke regex matched `failed` but not the new
  `failure` diagnostics, then its generic `abort` term rejected the new
  positive data-abort marker. The gate now rejects the common `fail` stem plus
  explicit panic/unhandled diagnostics, so a trap shutdown cannot pass while a
  recovered abort can.
- The process gate uses one MiB sections deliberately. It establishes the
  privilege and address-space contract before introducing second-level
  small-page allocation for ELF segment permissions and demand paging. Domain
  0 stays in client mode, so AP permission checks apply rather than being
  bypassed by manager-domain access.
- The shared VM policy now also encodes ARMv7 coarse-table and small-page
  descriptors. User leaves are non-global normal memory with explicit
  read-only/read-write and execute/execute-never combinations. The host gate
  locks their exact descriptor bits and rejects misaligned tables/pages,
  device-window aliases, out-of-RAM physical addresses, invalid permission
  values, and duplicate entries. The QEMU gate performs a user-mode
  read/write through a writable execute-never 4 KiB leaf. This layer accepts
  caller-owned table storage deliberately; allocation, reclamation, and
  copy-on-write belong to the generic memory backend.
- A coarse table needs only 1 KiB, but `kmalloc()` allocates and frees complete
  4 KiB pages. The ARM walker uses one allocator page per populated 1 MiB user
  region, installs its first 1 KiB as the coarse table, accounts that page in
  process RSS, and reclaims it only after all 256 leaves are absent. This
  wastes up to 3 KiB per populated region but preserves allocator ownership
  and alignment without adding a sub-page allocator.
- The generic x86 `PAGE_NOALLOC` value is bit `0x200`, which is ARM small-page
  AP2 rather than a software bit. Encoding it in a leaf would silently change
  user permissions. The ARM backend rejects `PAGE_NOALLOC` mappings; the only
  current caller is the framebuffer path, and ARM disables that i386-specific
  device. A future framebuffer port needs separate mapping metadata.
- The first embedded ELF mapped only bytes beginning at file offset 4096, so
  its program-header table was not present in any user mapping. That was enough
  for the bespoke copy loop but cannot supply a valid `AT_PHDR` to generic
  exec. Its single `PT_LOAD` now starts at file offset zero and maps the header
  page at `0x00100000`, while the code entry moves to `0x00101000`. A shared
  pure planner replaces duplicate parser logic and rejects malformed class,
  machine, type, sizes, file/virtual ranges, alignment, overlap, interpreter,
  entry, and program-header mappings before any process image is released. ARM
  execution is currently A32-only, so the planner also rejects a Thumb or
  otherwise non-word-aligned entry point.
- Generic ARM exec deliberately supports only static `ET_EXEC` images at this
  milestone. It eagerly populates anonymous fixed mappings through the normal
  VM backend, then builds an 8-byte-aligned AArch32 initial stack with the
  program-header, page-size, entry, and credential auxiliary-vector entries.
  `PT_INTERP`, PIE, demand paging, and Thumb entry remain outside this first
  bootstrap path. Reserving `(ARG_MAX + 1)` pages below the signal trampoline
  keeps every planned segment and the heap below the maximum argument/stack
  region.
- Writing executable pages after the initial MMU transition left stale
  instruction-cache and branch-predictor state as an undocumented correctness
  risk. The generic loader now executes an ordered DSB, whole instruction-cache
  invalidation, branch-predictor invalidation, DSB, and ISB after copying all
  loadable segments. The host loader gate requires exactly one invalidation,
  while the dual-version QEMU gate executes the same helper in the
  freestanding process oracle.
- The common signal unit previously treated every non-RISC-V architecture as
  i386, so an ARM pending signal would cast the 72-byte native trap frame to an
  unrelated x86 `sigcontext` and overwrite it. ARM now has an explicit branch
  and fixed-width Linux frame implementation. This first bootstrap policy
  always builds the real-time frame and returns through the kernel-provided
  A32 trampoline, including for one-argument handlers; alternate signal
  stacks, floating-point state, Thumb handlers, and caller-provided restorers
  remain unsupported.
- The fixture vectors used fixed IRQ/abort stacks and deliberately advanced
  data-abort return PCs to skip probe instructions. Neither behavior is valid
  for a schedulable process: a context switch would strand its frame on a
  global exception stack, and a resolved page fault must retry the faulting
  access. Generic non-SVC entries use ARMv7 `SRS` to place adjusted PC/CPSR
  state directly on the current process's banked SVC stack before building the
  common frame. Data abort subtracts eight from `lr_abt`; prefetch, undefined,
  and IRQ entries subtract four; SVC already contains the following PC. The
  standalone runtime gate locks the frame size and proves the SVC save/restore
  path without changing the fixture-only vector behavior.
- The original process root was assembled and activated as unrelated loops in
  `main.c`, and `CONFIG_ARCH_ARM` silently selected the i386 TSS layout in
  `arch_process.h`. The shared ARM VM API now checks root alignment and mapping
  bounds, reserves the low supervisor device slots, centralizes the kernel
  mapping template and TTBR0 transition, and provides an independently mutable
  clone operation. A host gate verifies kernel inheritance, user execute-never
  policy, rejection paths, clone isolation, and the ARM-specific process
  context. The QEMU gate then changes TTBR0 between two roots whose probe
  mappings differ, invalidates the complete unified TLB, verifies both mappings,
  and restores the primary root before user entry.
- `kmalloc()` cannot return the 16 KiB contiguous, 16 KiB-aligned block required
  by an ARM short-descriptor root; requests larger than one 4 KiB page are
  rejected. The generic ARM ownership layer therefore uses a fixed aligned
  pool sized to `NR_PROCS` instead of pretending four unrelated page
  allocations are contiguous. The 64 roots consume 1 MiB of aligned `NOBITS`
  storage plus owner metadata, trading bounded RAM for deterministic bootstrap
  behavior without increasing the disk image. Owner pointers and TTBR0 values
  are both checked on lookup/release, and the host gate covers exhaustion,
  deterministic reuse, clean child user roots, forged owners, and activation.
- Context switching intentionally does not write TTBR0 in the register
  save/restore routine. The generic scheduler must activate the incoming
  process root, update `current`, and only then move to the incoming stack. A
  host layout gate locks the assembly offsets to the 48-byte C structure, while
  the QEMU gate switches away and back twice so both first entry and saved-LR
  resumption are exercised.
- Generic kernel task setup allocates an aligned privileged stack and a process
  root, while first user entry preserves that SVC stack before programming the
  USR register bank. Fork setup creates a kernel-only root, copies the exact
  72-byte trap frame to the child stack, forces child r0 to zero, and resumes
  through the same exception-frame layout used by the live vector code. These
  hooks and the ARM page-cloning backend are cross-compiled and host-gated now;
  runtime fork awaits the linked generic ARM image.
- The historical init trampoline is a C function copied to a user page. On ARM
  that copy would retain PC-relative calls to kernel helpers and absolute
  references to kernel `argv`/`envp`, so it is not position-independent. ARM
  uses a standalone assembly blob whose local strings and vectors are resolved
  relative to its copied PC and user stack. The gate rejects relocations,
  requires start/end symbols, limits the blob to one page, and cross-compiles
  the complete generic PID 1 construction path.
- The first process-fork hook shallow-copied the parent's complete first-level
  root before generic `clone_pages()` ran. That worked for the isolated section
  gate, but would make parent and child refer to the same mutable second-level
  tables. Process roots now start with only the supervisor template; the
  architecture `clone_pages()` backend will be solely responsible for child
  user leaves and copy-on-write sharing.
- Generic process files previously used a binary RISC-V-versus-i386 split, so
  selecting `CONFIG_ARCH_ARM` reached `cr3`, TSS, and x86 stack-frame fields.
  Process initialization/release, task creation, scheduling, and fork now have
  explicit ARM branches, and a cross-compile gate covers all three generic
  translation units. Fork can construct a child context and clone private
  pages with copy-on-write, but executing it from PID 1 remains a runtime
  milestone for the linked generic ARM image.
- ARM also inherited the i386 `PAGE_OFFSET` conversion: adding `0xc0000000` to
  QEMU RAM at `0x40000000` wraps a 32-bit pointer to zero. ARM now defines an
  identity-mapped physical/kernel range beginning at `0x40000000`, uses that
  address as the user ceiling, and starts generic `mmap()` search at
  `0x20000000`. The VM host gate checks physical-page indexing and both address
  conversions before exercising descriptors.
- The common memory unit also treated every non-RISC-V target as i386. Its
  first ARM compile selected `cr3` walkers and omitted the ARM kernel-size
  macros. The architecture page operations now live in `arch/arm/memory.c`,
  while shared memory initialization has an explicit ARM branch that caps the
  current model at 128 MiB, reserves a 16 KiB root after BSS, installs the
  supervisor template, and activates it. Initrd pointers stay identity-mapped
  rather than receiving the i386 `PAGE_OFFSET` addition.
- The first task-hook header edit accidentally placed the `arm_trap_frame`
  forward declaration inside the RISC-V preprocessor branch. The strict ARM
  host compile rejected the resulting prototype-scope tag before it could
  become an ABI mismatch; the declaration now lives beside the ARM context.
- ARM SVC writes the following instruction address to `lr_svc`; the saved
  process PC therefore needs no explicit four-byte advance. The EABI translator
  leaves it unchanged, unlike the RISC-V ecall translator, and the host gate
  asserts that contract while checking arguments and signed errno returns.
- D-cache remains disabled after the MMU transition until the generic ARM port
  has an explicit cache-clean and aliasing contract. The instruction cache is
  invalidated before enabling it, after the user fixture has been copied.
- The relocated timer wait changed from testing a memory flag for zero to
  testing a saved register for one, but initially retained `beq`. It therefore
  printed the positive timer marker before any IRQ. The terminal assertion
  still observed `arm_timer_fired == 0` and rejected the run; the loop now uses
  `bne` and the smoke test requires the permission-abort marker as well.
- Wrapping the fixture in a page-aligned ELF moved its position-independent
  BSS reference beyond the immediate shape accepted by the `adr` pseudo-op.
  GNU `as` rejected the image rather than synthesizing a second instruction;
  the fixture now uses `adrl` for that arbitrary in-section displacement.
- The first ELF BSS began immediately after a 401-byte payload. With
  `SCTLR.A` enabled, the fixture's word-sized BSS check correctly faulted on
  the odd address before testing loader clearing. Padding `p_filesz` to a
  4-byte boundary gives BSS the alignment required by that check while
  retaining a larger `p_memsz`.
- The shared startup path treated every architecture other than RISC-V as
  i386. ARM therefore reached TSS/CR3 setup, BIOS memory-map page reservation,
  x86 I/O-permission state, and the PC reboot path. ARM now has an explicit
  generic startup, physical-page reservation policy, `ENOSYS` I/O-port policy,
  and PSCI reset path.
- Shared non-RISC-V guards also retained `/dev/port`, PS/2, PIT/CMOS, PC
  serial/parallel, floppy, and ATA code for ARM. These are i386 facilities, not
  generic fallbacks. The guards now classify both RISC-V and ARM as non-PC,
  and the link gate rejects any surviving legacy port-I/O boundary.
- The first complete link used the host hard-float libgcc with soft-float
  kernel objects. GNU ld correctly rejected the VFP argument ABI mismatch.
  The gate accepts an explicit matching runtime, and ARM supplies the EABI
  integer divide-by-zero hooks so libgcc does not import userspace `raise()`.
- Clang emitted an unaligned word store while formatting the first `printk`.
  QEMU's reset alignment controls turned it into a recursive data-abort before
  the generic console existed. Startup normalizes SCTLR alignment controls,
  and all ARM C builds use `-mno-unaligned-access` so correctness does not
  depend on firmware or hypervisor reset state.
- Fixture-only context-switch assertions originally occupied the same text
  section as the scheduler entry points, retaining their failure loop in the
  generic image. The fixture gate now has its own section, allowing
  `--gc-sections` to discard it while keeping the shared context primitives.
- The complete generic image compiles all 271 ARM/common C units with
  per-function/data sections, links the ARM boot/vector/context assembly, and
  rejects undefined symbols, writable-executable load segments, external
  network hooks, and legacy port-I/O hooks. Its QEMU gate proves entry, trap
  installation, CPU/IRQ core setup, physical-page initialization, process
  table setup, independent idle-root activation, PL011 system-console output,
  firmware-DTB reservation, writable ext2 over legacy and modern virtio,
  three GICv2-delivered physical timer ticks, static ELF32 exec, signal return,
  fork, copy-on-write, wait4, and PSCI reset from PID 1.
- The generic Make target exported an empty `GENERIC_CC_TARGET` for GCC, but
  `${GENERIC_CC_TARGET:---target=arm-linux-gnueabihf}` treated empty as absent
  and passed Clang's `--target` option to the bootstrap-built GCC. The scripts
  now distinguish an unset variable from an explicitly empty one, preserving
  the Clang default without making the generic image Clang-only.
- Bootstrap binutils 2.30 rejected the GCC-built final image because libgcc's
  ordered `.ARM.exidx` input and ordinary unordered constants were assigned to
  the same `.rodata` output section. Fiwix has no exception or stack-unwind
  consumer, so the generic linker script now discards `.ARM.exidx` and
  `.ARM.extab` alongside the already-discarded `.eh_frame` instead of retaining
  dead unwind metadata or requiring a newer linker.
- The image gate required the internal `arm_fdt_parse` symbol by its source
  name. GCC correctly specialized its only live call as a local
  `arm_fdt_parse.part.0`, so the linked implementation was present but the
  Clang-shaped symbol assertion failed. The gate now requires the public
  `arm_fdt_boot_discover` kernel boundary and leaves private optimization names
  to the compiler.
- Bootstrap ARM TinyCC selects softfp code generation itself and rejects
  GCC's `-mfloat-abi=soft` spelling. `GENERIC_FLOAT_ABI` makes that one
  compiler-driver distinction explicit while keeping the source flags, source
  list, linker policy, and runtime boot gate shared with GCC and Clang.
- The bootstrap TinyCC compiler is a chain intermediate rather than a complete
  installed SDK, so its compiler-private `stdarg.h` is not found implicitly.
  `TCC_INCLUDE` makes that exact source-tree header an explicit input; Fiwix
  does not copy TinyCC's variadic ABI definitions or fall back to host headers.
- ARM TinyCC's EABI runtime memory helpers call the standard freestanding
  `memcpy`/`memset` entry points. Those aliases already wrapped Fiwix's
  `memcpy_b`/`memset_b` for the RISC-V TinyCC image; ARM now shares the same
  lowering boundary instead of importing a userspace libc.
- Bootstrap TinyCC ignores `-ffunction-sections`, so a common source object
  retains unreachable references to the PC `inport_b`/`outport_b` boundary.
  As on RISC-V, weak sentinels make the image link and a checked expected file
  fixes the exact retained set; any new port-I/O or external-network reference
  remains a hard link-gate failure.
- Random `mktemp` object paths entered the linked ELF's `STT_FILE` strings, so
  its loaded bytes were reproducible while its complete file hash was not.
  The TinyCC target now uses the owned, stable
  `.generic-package-work` directory through `GENERIC_WORKDIR`, matching the
  RISC-V package boundary and making the complete ELF reproducible.
- TinyCC's bounded ARM integrated assembler does not implement privileged
  `mcr`/`mrc` instructions or even the inline `nop` used by dormant PC drivers.
  The remaining generic timer, MMU, barrier, and no-operation primitives now
  live in `ops.S`, which is assembled by bootstrap GNU `as`, while the C
  compiler sees ordinary function calls. This keeps machine instructions in
  one architecture-owned assembly boundary.
