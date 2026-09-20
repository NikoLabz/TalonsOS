# TalonsOS Audit Report

**Scope:** the `TalonsOS_tar.gz` tarball you uploaded (kernel/, filesystem/talonsfs/, boot/).
**What I did:** extracted it, compiled every source file with real freestanding flags,
linked a full kernel, built a bootable disk image, and booted it in QEMU — then traced
a crash to its root cause and fixed it.

---

## 1. What I actually did (in order)

1. Extracted the tarball and reviewed every source file by hand (kernel/, filesystem/talonsfs/, boot/).
2. Installed the missing toolchain in the sandbox (`nasm`, `grub`/`xorriso`, `qemu-system-x86`) —
   none of this was available or included with the project.
3. Compiled every `.cpp` file individually with `-ffreestanding -fno-exceptions -fno-rtti
   -mno-red-zone -m64` to catch real compiler errors, not guessed ones.
4. Attempted a full link of the kernel objects + relevant TalonsFS library objects.
5. Assembled `boot.asm` and `entry.asm` with `nasm`, built a flat kernel binary, wrote a
   boot sector with the correct `KERNEL_SECTORS` value, and assembled a raw disk image.
6. Booted the disk image in headless QEMU. First attempt: **silent crash, no output.**
7. Diagnosed the crash using QEMU's exception tracer (`-d int`) and GDB attached to
   QEMU's remote stub, comparing against a blank-disk control run to confirm the crash
   was caused by the kernel, not the environment.
8. Found the cause (`-mcmodel=large`, see below), fixed it, rebuilt, reboots successfully.
9. Confirmed success by dumping the raw VGA text buffer over GDB and decoding it — not
   just "no crash," but literally read back `TALONSOS KERNEL ONLINE.` and the shell
   prompt from guest memory.
10. Wrote a working `Makefile` and `linker.ld` reflecting everything verified in steps 3–9.
11. Cross-checked the project's `TODO.md` against the actual source and corrected checkbox
    states that were stale (items marked undone that are in fact implemented, and one
    important gap — no IDT — that wasn't tracked anywhere).

---

## 2. Files delivered

| File | Status |
|---|---|
| `Makefile` | **New.** Reconstructed from scratch — none was in the tarball. Builds boot sector, kernel, disk image, and a `make run` target. |
| `linker.ld` | **New.** Places `_start` at `0x1000`, matching what `boot.asm` expects. *(Superseded: the kernel now loads and links at `0x10000` — see the boot-layout notes in `TODO.md`.)* |
| `TODO.md` | **Updated.** Checkbox corrections + a new "Current Status" section documenting this audit. |

None of your original `.cpp`/`.h`/`.asm` source files have been modified yet — see
Section 4 for what's still pending in your actual source tree.

---

## 3. Bugs found and fixed

### 3.1 Boot-hang: `-mcmodel=large` (fixed, root cause confirmed)

**Symptom:** kernel compiled and linked with zero errors, but crashed immediately on
boot — no VGA output, no error, just silence.

**Diagnosis:**
- `-d int` tracing showed an infinite storm of `#UD` (invalid opcode, exception vector 6)
  starting almost immediately after entering long mode.
- A control test booting a completely blank disk produced **zero** such exceptions in
  the same window, confirming this was caused by the kernel itself, not the QEMU/BIOS
  environment.
- GDB inspection showed the CPU parked back at the BIOS reset vector — consistent with
  a triple fault (an unhandled exception with no IDT causes a CPU reset back to the
  firmware).
- Disassembling the compiled kernel showed GCC generating PC-relative-plus-64-bit-immediate
  address computations for function calls — the codegen pattern `-mcmodel=large` produces.
  That pattern needs GOT-style relocation handling that a plain linker script doesn't
  provide, so the computed call targets were garbage, and jumping into a garbage address
  (likely landing in the middle of ASCII string data) is exactly what produces `#UD`.

**Fix:** recompiled with `-mcmodel=small` instead. Rebooted — confirmed working by
reading back `TALONSOS KERNEL ONLINE.  TalonsFS: mounted` and the shell prompt directly
from VGA memory.

**Why `small` is correct here:** your kernel loads at a fixed low address (`0x1000` at the time of this audit; now `0x10000`),
well within the 32-bit range `-mcmodel=small` assumes. `-mcmodel=large` exists for code
that might be linked far apart in a >2GB address space — not the case for a flat
hobby-OS kernel loaded near address zero. This is already baked into the `Makefile` I
delivered.

**Caveat:** I don't know what your actual build previously used, since no Makefile was
in the tarball. If your real build already used `-mcmodel=small` (or no `-mcmodel` flag
at all, which defaults to `small`), this specific crash may not apply to you — but it's
worth checking, since it's exactly the kind of bug that produces a silent, undiagnosable
reset.

### 3.2 Duplicate `kmalloc()` — link-time collision (found, not yet fixed in your source)

`kernel/memory.cpp` defines `kmalloc(std::size_t)`. A second, unused file,
`kernel/allocator.cpp`/`allocator.h`, defines `kmalloc(uint64_t)`. On x86-64, `size_t`
and `uint64_t` are both `unsigned long`, so these two functions mangle to the **identical**
linker symbol. Linking both together fails with `multiple definition of 'kmalloc'`.

Confirmed by actually linking both `.o` files and reproducing the exact linker error.

Only `memory.cpp` is wired into anything — `new_delete.cpp` and `kernel.cpp` both
`#include "memory.h"`. Nothing includes `allocator.h`. It's dead code, currently harmless
only because nothing links it in.

**Recommended fix:** delete `kernel/allocator.h` and `kernel/allocator.cpp`. Not yet
applied to your source — I only excluded them from the Makefile I delivered.

### 3.3 `memory_init()` never called (found, not yet fixed)

`kernel.cpp` includes `memory.h` but `kernel_main()` never calls `memory_init()`. It
currently works by accident: the allocator's state is in an anonymous-namespace global,
so it's zero-initialized by default BSS behavior, which happens to match what
`memory_init()` would set anyway. Fragile, not urgent, but a one-line fix
(`memory_init();` as the first line of `kernel_main()`).

### 3.4 No IDT anywhere in the kernel (found, architectural gap — not a "fix")

Nothing in the codebase loads an Interrupt Descriptor Table. This means any CPU
exception after entering long mode — divide-by-zero, a bad pointer dereference, an
invalid opcode, anything — has no handler, and the CPU triple-faults, which resets the
whole machine with **zero diagnostic output**. This is exactly what made bug 3.1 hard to
find: I only located it by attaching an external debugger (GDB) to the emulator from
outside the OS, since TalonsOS itself has no way to report what happened.

This isn't a bug in the traditional sense (nothing currently in the code is "wrong"
because of it), but it's the single biggest thing currently making TalonsOS hard to
debug as it grows. Flagged as the top of Phase 6 in the updated `TODO.md`.

### 3.5 `shell.cpp` sign-compare warnings (found, cosmetic, not yet fixed)

Three instances of comparing a signed `int` loop counter against `kfs::MAX_NAME_LEN - 1`
(unsigned) trigger `-Wsign-compare`. Harmless in practice given the actual value ranges
involved, but easy to clean up by making the loop counters `unsigned int`.

### 3.6 `directory.cpp` can't compile freestanding (found, not currently a live problem)

`filesystem/talonsfs/src/directory.cpp` includes `<vector>`, `<algorithm>`, `<limits>` —
hosted-only C++ headers requiring a working heap/exceptions. Confirmed this by actually
trying to compile it with the kernel's freestanding flags — it fails immediately.

This isn't currently breaking anything: your kernel's own `filesystem.cpp` never
includes `directory.h` and talks directly to the lower-level managers instead
(`NodeManager`, `NodeDataManager`, etc.). But it's an undocumented trap — if anyone
later includes `directory.h` from kernel code, the build breaks with a confusing
freestanding-header error rather than an obvious "wrong file" error.

**Recommended fix:** a one-line comment in `directory.cpp`, `filesystem.cpp` (the
TalonsFS::Filesystem class, not the kernel one), `formatter.cpp`,
`memory_block_device.cpp`, and `file_block_device.cpp` marking them host-only
(used by the test suite / mkTalonsFS tool, never linked into the kernel).

---

## 4. What's still pending in your actual source

Nothing above except the Makefile/linker.ld/TODO.md has been written back to your
project yet. Still open, in rough priority order:

1. Delete `kernel/allocator.h` + `kernel/allocator.cpp` (3.2)
2. Add `memory_init();` to the top of `kernel_main()` (3.3)
3. Fix the three sign-compare warnings in `shell.cpp` (3.5)
4. Add host-only comments to the five TalonsFS files listed in 3.6
5. (Bigger scope, not a quick fix) Build an IDT + basic exception handlers — see 3.4

Say the word and I'll make these edits directly and hand you back an updated tarball,
or just the changed files individually if you'd rather apply them yourself.

---

## 5. What's confirmed working, end to end

Verified by an actual boot, not just a successful compile:

```
TALONSOS KERNEL ONLINE.  TalonsFS: mounted
Developer@TalonsOS>
```

Confirmed working: real-mode boot → GDT load → protected mode → long-mode page tables
→ long-mode GDT → `kernel_main()` → TalonsFS mount via the real ATA block device path
→ terminal init → shell prompt render. This is the full boot chain your project
describes in its own documentation, and it works.
