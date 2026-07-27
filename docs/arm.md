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

Milestones 1 and 2 are implemented on this branch, and milestone 3 now has its
first process-address-space gate. The ARM VM layer owns 16 KiB ARMv7
short-descriptor roots, seeds supervisor-only RAM/device mappings, validates
user section mappings, derives TTBR0, activates a process root with a complete
TLB flush, and clones roots independently for the future fork path. The boot
oracle switches between roots at `0x47e00000` and `0x47e04000`; each maps
virtual `0x00300000` to a different physical section, and the QEMU gate checks
both values before restoring the primary root. It copies a position-independent
ARM EABI fixture to `0x47000000` and maps it at user virtual `0x00100000`. A
separate execute-never user stack maps `0x47100000` at
`0x00200000..0x002fffff`.

The process fixture is a standalone static ELF32/ARM `ET_EXEC` embedded in the
bring-up image. The bounded loader validates its ELF identity, machine, type,
header sizes, program-header table, entry point, source ranges, user virtual
ranges, and `p_filesz <= p_memsz`; it copies every `PT_LOAD` segment and clears
BSS before enabling translation. The same loader still needs to be connected
to filesystem-backed generic exec.

The ARM EABI-to-generic syscall policy is also host-gated. Legacy ARM numbers
for the bootstrap file/process calls match Fiwix's i386-indexed table, but the
translator still dispatches them explicitly so unsupported entries do not
become accidental ABI. It covers `exit`/`exit_group`, fork-compatible `clone`,
read/write/open/openat/close, exec, wait4, cwd and basic pathname operations,
descriptor duplication, brk, process IDs, kill, sync, and reboot. The `*at`
subset currently accepts only `AT_FDCWD`; `unlinkat` rejects flags, and
`clone` accepts only `SIGCHLD` with a null child stack. The freestanding oracle
does not link the generic syscall table, so this unit remains host-tested until
the generic ARM image is introduced.

The ARM `arch_context` now records callee-saved registers, the scheduler
continuation, TTBR0, and the privileged stack instead of falling through to
the i386 TSS layout. The AArch32 switch primitive saves and restores r4-r11,
SP, and LR; TTBR0 activation remains a separate ordered operation for the
scheduler. Its QEMU gate runs an alternate continuation twice on an independent
stack and verifies every callee-saved sentinel after resumption. A generic
ownership layer reserves one aligned root for each of Fiwix's 64 process slots,
associates every allocated root with its `struct proc`, rejects forged
release/activation requests, clones a parent root for the future fork path, and
deterministically reuses released slots. It is host-gated but is not linked
into the freestanding oracle; the generic ARM image will initialize the pool
and connect these hooks to process creation, release, and scheduling. Root
cloning gives each process an independently mutable descriptor table, but it
deliberately still shares the mapped physical sections; physical page
allocation and copy-on-write belong to the generic fork integration.

The fixture preserves a complete register frame across SVC, alignment abort,
section-permission abort, and IRQ. It proves that USR mode cannot read the
identity-mapped kernel at `0x40010000`; both abort handlers and the timer IRQ
signal completion by modifying the saved user frame. Generic `struct proc`
allocation/context switching, private physical pages, filesystem access, and
bootstrap execution remain incomplete.

The Clang ELF32 process oracle is 16,388 bytes with SHA-256
`a96d70cd3420d092da32a14f09e502075bd66db377b56180ff8c710b9df00ee5`.

## Design and bug log

- The first ARM source additions used `GPL-2.0-or-later` SPDX tags copied from
  an unrelated convention even though Fiwix uses its own project license.
  Every ARM source, public header, and linker-script addition now carries the
  same Fiwix License notice used by the repository's architecture code.
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
- Console output uses PL011 polling. Interrupt-driven console and GIC setup
  belong to the trap milestone, not the boot oracle.
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
  deterministic reuse, parent cloning, clone isolation, forged owners, and
  activation.
- Context switching intentionally does not write TTBR0 in the register
  save/restore routine. The generic scheduler must activate the incoming
  process root, update `current`, and only then move to the incoming stack. A
  host layout gate locks the assembly offsets to the 48-byte C structure, while
  the QEMU gate switches away and back twice so both first entry and saved-LR
  resumption are exercised.
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
