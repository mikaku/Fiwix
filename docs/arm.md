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

Only milestone 1 is implemented on this branch. No userspace, filesystem, or
bootstrap completion claim follows from the boot marker. The current Clang
oracle produces a 4,100-byte raw image with SHA-256
`49b8cd0b18fda50a26889579dad07ceeb7e0c6d290f9d28ddb60e386964d1caf`;
the hash is an implementation anchor, not yet a bootstrap acceptance hash.

## Design and bug log

- ARM has two relevant execution states in this bootstrap. Fiwix deliberately
  targets ARMv7 after the existing AArch64-to-AArch32 compiler pivot rather
  than introducing an unproven ARMv7 seed or a new AArch64 Mes backend.
- QEMU may enter a 32-bit kernel in HYP mode when virtualization is enabled.
  The entry code preserves `r2`, programs `ELR_hyp`/`SPSR_hyp`, and enters
  masked SVC mode before touching C state.
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
