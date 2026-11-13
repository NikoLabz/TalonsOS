# TalonsOS Phase 5 - Memory Management

Working document. Updated at the end of every milestone.

## Milestone status

| M | Name | Status |
|---|------|--------|
| M1 | Hygiene + memory map | Implemented and host-tested. QEMU normal boot verified during M4; `meminfo`/`memmap` command output has not been separately rechecked in this run. |
| M2 | PMM hardening | Implemented and host-tested. QEMU normal boot verified during M4; allocator-specific runtime scenarios remain untested. |
| M3 | Kernel heap redesign | Implemented and host-tested. QEMU normal boot verified during M4; allocator stress/corruption scenarios remain host-only. |
| M4 | Minimal IDT + page fault | Implemented. Full host suite, freestanding link, size check, normal QEMU boot and controlled page-fault diagnostic passed. |
| M5 | Paging / VM + protection | Implemented and verified in host tests and QEMU |
| M6 | User-space memory | Implemented; host and QEMU verification recorded below |
| M7 | Debug tools + OOM final | Implemented; verification results recorded below |

## Verification levels (used in every report)

1. **Host tests** - `make test`; native g++, real kernel sources where noted.
2. **Freestanding compile/link** - the real `CXXFLAGS` (`-m64 -ffreestanding ... -Wall -Wextra`) and `linker.ld`; `make size-check` performs the link and checks both kernel limits.
3. **QEMU boot** - `make run`. Only counts if it was actually run.

## Architectural decisions

### AD-1: Memory map logic is pure, header-only and host-testable (M1)
`kernel/memory_map.h` (summary of the E820 map) and `kernel/memory_format.h`
(decimal/hex/size text) have no hardware access, matching the existing
`e820.h` / `paging_internal.h` pattern. `kernel/meminfo.cpp` is the thin
kernel-side layer that renders reports to a file descriptor.

### AD-2: Additive E820 accessors (M1)
`e820_header()` and `e820_entries()` were added to `e820.h` / `e820.cpp`.
They return `nullptr` until `e820_init()` has validated the boot map. No
existing function changed. `kernel.cpp` still uses its own hard-coded
`0x5FF0` / `0x6000` and was not modified (it carries uncommitted work).

### AD-3: Summary semantics (M1)
* Totals are raw sums of entry lengths and saturate at `UINT64_MAX`.
* Entries with attribute bit 0 clear are *disabled*: reported separately,
  never counted as usable or reserved.
* `hasOverlap` considers every non-empty entry, matching `frame_allocator`
  (which treats any overlap as ambiguous). If true, totals may double count.

### AD-4: Shell commands (M1)
`meminfo` and `memmap` follow the existing `talonc` pattern: implementation
in its own file, declaration in a header, command added to
`execute_command()` in `shell.cpp`, no-op stubs added to
`tests/host_stubs/shell_command_stubs.cpp` for the navigation test. Both
write to the caller's stdout/stderr descriptors, so redirection works.

## M1 - Hygiene + memory map

### Implemented
* `memory_map.h`: `typeName()`, `Summary`, `summarise()`.
* `memory_format.h`: `formatDecimal()`, `formatHex()`, `formatSize()`.
* `meminfo.h/.cpp`: `meminfo_run()`, `memmap_run()`.
* `e820.h/.cpp`: `e820_header()`, `e820_entries()`.
* `shell.cpp`: `meminfo` and `memmap` commands, two lines in `TalonsHelp`.
* `Makefile`: `meminfo` in `KERNEL_SRCS`; four tests added to `make test`;
  `make size-check`.
* `tools/check_kernel_limits.sh`: checks the 123-sector image limit and the
  `_kernel_end <= 0x80000` limit. Exits 1 on violation (negative case tested).
* `kernel/test_memory.cpp` (already in the tree, previously unwired) is now
  part of `make test`. It still asserts the 256-slot limit; M3 will change
  that assertion deliberately.
* `linker.ld`: comment-only fix. It said 127 sectors; `stage2.asm` enforces
  123 (`MAX_KERNEL_SECTORS`), LBA 5..127.

### Not changed (deliberately)
* No file deleted, reset or committed. Uncommitted work (paging, `entry.asm`,
  `kernel.cpp`, `Makefile`, `vga_stub.cpp`) is preserved.
* Stray files `paging.cpp.vmm-backup`, `kernel/.terminal.cpp.swp` and
  tracked `build/mkTalonsFS` are preserved. The M7 full project archive
  includes the complete tree, including `.git` and empty directories.
* At M1 time, `kernel.cpp` reserved ranges, frame allocator, heap, paging were untouched.

### Expected `meminfo` on a default QEMU guest (for your boot check)
Values below are what the host integration test produces for a typical QEMU
128 MiB map; real output depends on your QEMU settings.

```
Physical memory (BIOS E820)
  Entries:         6
  Usable:          133692416 bytes (127 MiB)
  Reserved:        459776 bytes (449 KiB)
  Highest usable:  0x07FE0000 (end, exclusive)
  Overlaps:        no
Frame allocator (4096-byte frames)
  Free frames:     <N> (<size>)
  Tracked up to:   0x40000000
Kernel
  Image end:       0x000xxxxx
```

### Limits after M1 (real link with the prebuilt `entry.o`; supersedes the earlier approximate figures)
| | Upload baseline | After M1 |
|---|---|---|
| Image | 37,080 B / 73 sectors | 39,832 B / 78 sectors |
| Sector headroom (max 123) | 50 | 45 |
| `_kernel_end` | 0x6E101 | 0x6EBC1 |
| Headroom to 0x80000 | 73,471 B | 70,719 B |

Reproduce with `make size-check`.

### Known limitations
* QEMU boot unverified (see top).
* `e820_header()`/`e820_entries()` read fixed physical addresses, so they are
  covered by freestanding compile only; the report code that uses them is
  covered by `test_meminfo` through stubs.
* No E801/88h fallback if BIOS E820 fails; boot stops with a message.
* `memmap` prints one line per entry (up to 64) with no paging.

## M2 - PMM hardening

### Implemented (all in `kernel/frame_allocator.h/.cpp`; existing API and semantics unchanged)
* **Frame 0 is always reserved by `init()`.** `allocate()` uses 0 as its failure value; previously a map with usable memory at address 0 and no caller reservation could hand out frame 0, indistinguishable from failure. (Behaviour change, deliberate; no existing test depended on it. In the real boot, `kernel.cpp` already reserves 0..0x10000.)
* **Search hint.** `allocate()` no longer scans from frame 0 every call. Invariant: no free frame lies below `search_hint`; `release*()` lowers it. Result is still always the lowest free frame (checked against a reference model).
* **Byte-skipping scan.** `find_free()` skips fully used bytes eight frames at a time.
* **`Stats` / `getStats()`**: managed, free, allocated, largest free run, lifetime frames allocated/released, failed allocation calls, rejected release calls, usable bytes above the 1 GiB tracking limit.
* **`allocateContiguous(count)` / `releaseContiguous(addr, count)`**: lowest-address fit; release is all-or-nothing.
* **`isAllocated()` / `isFree()`** queries.
* **`verify()`**: O(MAX_FRAMES) bookkeeping self-check.
* **`meminfo`** now shows managed/allocated frames, largest free run, lifetime counters, failures, untracked memory (only if non-zero) and the self-check result. Existing `meminfo` lines are unchanged.

### Tests
* `test_frame_allocator` (existing): unchanged, passes.
* `test_frame_hardening` (new, #includes `frame_allocator.cpp` for white-box checks): frame 0, stats and counters, every kind of rejected release, usable memory above the limit (straddling and fully above), contiguous allocation around a reserved hole, all-or-nothing release, hint behaviour, `verify()` catching 7 kinds of corruption, and a model-based randomised test (8 seeds x 6000 operations against a reference model, `verify()` every 100 operations).
* `test_meminfo` (extended; existing assertions kept): new lines, counters tracking real activity, untracked line.
* Mutation check: 8 deliberate bugs injected into `frame_allocator.cpp`; 7 were caught. The 8th (`find_free(start + run + 1)`) is an equivalent mutant: frame `start + run` is always allocated, so skipping it changes nothing.

### Deliberately NOT done in M2
* **Zero-on-allocate.** Frames above 2 MiB are not mapped (boot identity map covers 0..2 MiB), so the kernel cannot write to an arbitrary frame yet. A "zeroed allocation" now would be a placeholder. Moved to M5 (needs the temporary-mapping window).
* **Boot reserved ranges (B12).** Moving the hard-coded table out of `kernel_main()` means editing `kernel.cpp`, which carries uncommitted work and cannot be boot-tested here. Deferred to M4, where `kernel.cpp` must change anyway for the IDT.
* **1 GiB tracking limit** is unchanged; memory above it is now *reported* (`untrackedUsableBytes`) rather than silently ignored.

### Limits after M2 (real link with the prebuilt `entry.o`)
| | After M1 | After M2 |
|---|---|---|
| Image | 39,832 B / 78 sectors | 41,624 B / 82 sectors |
| Sector headroom (max 123) | 45 | 41 (20,992 B) |
| `_kernel_end` | 0x6EBC1 | 0x6F301 |
| Headroom to 0x80000 | 70,719 B | 68,863 B |
| bss | 348,193 B | 348,257 B (+64: counters) |

### Known limitations
* QEMU boot unverified. In particular the new `meminfo` lines and the frame-0 reservation have only been exercised by host tests.
* `getStats()` and `verify()` each scan 262,144 frames; fine for a shell command, not for a hot path.
* Contiguous allocation has no alignment option yet.
* Lifetime counters are 64-bit and unsynchronised (single CPU, no interrupts yet; revisit when interrupts arrive in M4).

## M3 - Kernel heap redesign

### Search-hint verification (requested before M3)
`test_frame_hardening` gained `testHintExhaustive`: a 10-frame window with one reserved frame, **every one of the 2^9 allocation patterns**, built two ways (so the hint is driven up and down). For each pattern it checks (a) `allocateContiguous(1..4)` against a reference scan, and (b) for every allocated frame: release it (a release *below* the hint) and require the next `allocate()` to return the lowest free frame. Removing the "release lowers the hint" line makes this test alone fail. Result: the hint behaves correctly and lowest-fit is preserved. (`verify()` also checks "no free frame below the hint" and is run once per pattern.)

### Design
* `kernel/heap.h/.cpp`: the allocator core (`talons_heap::Heap`). No hardware or kernel dependencies, so it is tested on the host against plain buffers.
* `kernel/memory.h/.cpp`: binds one `Heap` to the kernel region (1 MiB at 0x100000, unchanged) and keeps the original API. `kmalloc`/`kfree`/`memory_init` signatures are unchanged.
* Implicit address-ordered block list, first fit (lowest address), always split, always merged: no two free blocks are ever adjacent.
* 32-byte header per block: state magic, payload size, size of the physically previous block (for backward merge), requested size, 32-bit checksum of those fields. Payload is 16-byte aligned.
* After the user bytes: slack filled with 0xA5, then an 8-byte canary (depends on block address and size).
* Free payloads are filled with 0xDD ("poison"). The whole region is poisoned at init.

### Architectural decisions
* **AD-5: the heap never repairs what it cannot validate.** Every header that will be read or rewritten is validated first (checksum, magic, size, back-link). A corrupted neighbour makes `kmalloc` return nullptr and makes `kfree` leak the block, both reported as `HeaderCorruption`. Leaking is chosen over "healing" with a wrong guess.
* **AD-6: error reporting.** Every detected problem is counted, remembered (`lastError`) and passed to an optional handler (`memory_set_error_handler`). Panic policy is deliberately not decided here (the panic facility comes with the IDT in M4). The handler may not call back into the allocator; re-entry of the handler itself is blocked.
* **AD-7: lazy initialisation.** The old allocator worked without `memory_init()` because all of its state was zero. The new one initialises itself on first use, so code that runs before `kernel_main` (global constructors) keeps working. `memory_init()` still resets the heap.
* **AD-8: `operator new`.** It used to return nullptr on failure, which the language does not allow callers to expect. It now never returns nullptr: on failure it calls `memory_fatal_out_of_memory()` (writes `KERNEL PANIC: OUT OF MEMORY` on the last VGA row and halts, unless a handler is installed). `operator new(0)` now returns a valid pointer. `new (std::nothrow)` forms were added and return nullptr. Audit: nothing in the kernel used plain `new` (only placement `new`), and `talonc`, the only direct `kmalloc` caller, checks for nullptr.
* **AD-9: host build flag.** `memory.cpp` is compiled with `-DTALONS_HOST_TEST` for host tests so the heap lives in a static buffer; the kernel build never defines it.
* **AD-10: classification of bad frees.** A pointer into the middle of an allocation is `InvalidFree`. A pointer to a block that was freed and merged away (its old header is poison and lies inside a free block) is `DoubleFree`.

### Behaviour changes (deliberate, approved)
1. The 256-allocation limit is gone. `test_memory.cpp` assertion 6 ("exactly 256 of 300 `kmalloc(1)` succeed") was replaced: all 300 succeed and the heap fills at exactly 21,845 allocations (48 bytes per `kmalloc(1)`), fails safely, and fully recovers when freed.
2. A single 1 MiB request can no longer succeed because headers live inside the heap. `test_memory.cpp` assertion 8 now uses `memory_max_allocation()` (1,048,536 bytes). `kmalloc(1024*1024) == nullptr` is asserted.
3. `operator new` behaviour as in AD-8.
4. Every other assertion in `test_memory.cpp` is unchanged.

### New API (memory.h, additive)
`memory_get_stats`, `memory_check`, `memory_max_allocation`, `memory_set_error_handler`, `memory_set_fatal_handler`, `memory_fatal_out_of_memory`.
`meminfo` gained a "Kernel heap" section: size, in use/peak, free/largest block, fragmentation percent, lifetime counters, OOM/double/invalid frees, corruption counters, self-check.

### Tests
| Test | Covers |
|---|---|
| `test_heap` (new) | init arguments, minimal heap, sizes 1..300 plus large, alignment, split/reuse, merging in all 24 free orders, exact maximum allocation, OOM (incl. SIZE_MAX), fragmentation (OOM with plenty of free memory; stats show it), 15,000 live allocations (old limit 256), double free (both header-intact and merged-away), 10 kinds of invalid free, slack/canary overflow (every canary byte), overflow into neighbour header, underflow, use-after-free, **every byte of an allocated and a free header flipped**, back-link/neighbour validation, forged self-consistent header, unmerged free blocks, statistics and peaks, handler re-entry guard, 6 random stress runs x 20,000 operations with data-integrity and no-overlap checks |
| `test_memory` (existing) | original assertions + the two deliberate changes above |
| `test_memory_api` (new) | lazy init, stats, check, error handler, fatal handler |
| `test_new_delete` (new) | the real `new_delete.cpp`: new/delete, arrays, sized delete, `new(0)`, nothrow, fatal path |
| `test_meminfo` (extended) | heap section text, counters follow activity, self-check turns FAILED after damage |

Mutation testing of `heap.cpp`: 26 injected bugs (checksum ignored, merges disabled, back-link not fixed, canary/slack/poison checks removed, validations of previous/next/after headers removed, off-by-one in max allocation and rounding, counters, fragmentation formula, ...). The first run left 4 survivors; tests were added for each (neighbour validation, forged back-link, unmerged free blocks); the final run killed **26 of 26**.

### Limits after M3 (real link with the prebuilt `entry.o`)
| | Baseline | M1 | M2 | M3 |
|---|---|---|---|---|
| Image | 37,080 B / 73 sectors | 39,832 B / 78 | 41,624 B / 82 | 45,912 B / 90 |
| Sectors left (max 123) | 50 | 45 | 41 | **33 (16,896 B)** |
| `_kernel_end` | 0x6E101 | 0x6EBC1 | 0x6F301 | 0x6EC61 |
| Left to 0x80000 | 73,471 B | 70,719 B | 68,863 B | 70,559 B |
| bss | | | 348,257 B | 342,273 B |

bss shrank by about 6 KB because the old 256-slot record table is gone. **The sector limit is now the tightest constraint**: M4 (IDT), M5 (paging protection), M6 and M7 must fit in 33 sectors, or the layout limit must be raised deliberately (stage2.asm `MAX_KERNEL_SECTORS` and the TalonsFS start at block 128 are tied to it). To be decided before M4.

### Known limitations
* QEMU boot was verified during M4. In particular `memory_init()` writes the whole 1 MiB heap (poison fill) at boot; the normal QEMU boot completed, but no physical-hardware run is recorded.
* First fit walks the block list: O(blocks) per allocation. Fine for a 1 MiB heap; revisit if the heap grows or becomes hot.
* No `kcalloc`/`krealloc`/aligned allocation. No locking: the heap is not safe to call from exception or interrupt handlers. The M4 handlers do not allocate or call heap routines.
* Canaries detect accidental overflow, not deliberate tampering (the value is derivable from the block address).
* A neighbour with a damaged header makes a free refuse (leak). That is by design (AD-5).
* The heap is fixed at 1 MiB at 0x100000, not tied to the E820 map.
* Use-after-free is only noticed when the poison of a free block is later examined (`memory_check()` or when that memory is allocated again), and only for the bytes being handed out.
* `memory_fatal_out_of_memory()` still uses its direct VGA halt path; it has not been unified with the M4 CPU-exception reporter.

## M3 amendment - `operator new` review (requested after M3)

Files: `kernel/new_delete.cpp`, `kernel/memory.cpp` (fatal path), `kernel/test_new_delete.cpp`.

### Behaviour of every form

| Expression | Operator | Success | Out of memory |
|---|---|---|---|
| `new T`, `new T(args)` | `operator new(size_t)` | constructor runs, pointer is 16-byte aligned | `memory_fatal_out_of_memory(size)` notifies the installed handler; if it returns, `memory_halt_out_of_memory(size)` halts. The constructor is **not** reached |
| `new T[n]` | `operator new[](size_t)` | as above | same. A length overflow arrives as `SIZE_MAX` and fails the same way (no wrap-around); tested |
| `new (std::nothrow) T` | `operator new(size_t, nothrow_t)` | as plain `new` | returns nullptr, the constructor does **not** run, the fatal path is **not** entered; tested with a 2 MiB object |
| `new (std::nothrow) T[n]` | `operator new[](size_t, nothrow_t)` | as above | nullptr |
| `new (ptr) T`, `new (ptr) T[n]` (placement) | `operator new(size_t, void*)` | **not defined here**: it is `inline` in `<new>` and returns `ptr`. The heap is never touched; tested (allocation, free and OOM counters unchanged) | n/a |
| `delete p`, `delete[] p` | `operator delete(void*)` | `kfree`; destructor runs before | pointer not owned by the heap (stack, double delete, interior pointer): detected, counted (`invalidFrees`/`doubleFrees`), ignored, never crashes; tested |
| `delete nullptr` | | no-op, no error counted | |
| sized / nothrow `delete` | `operator delete(void*, size_t)` etc. | `kfree` | |
| `new (std::align_val_t)` / over-aligned types (> 16) | **not defined** | link error | Deliberate: the heap guarantees 16-byte alignment only, and a link error is better than silent misalignment. Nothing in the kernel needs it |

`operator new(0)` returns a valid, unique pointer (it requests one byte; `kmalloc(0)` itself is still nullptr).

### The out-of-memory panic path
* `kmalloc` itself **never** panics: it returns nullptr (counted, `Error::OutOfMemory` to the error handler). Only the throwing `operator new` forms escalate.
* The escalation first calls `memory_fatal_out_of_memory(size)` as a notification. If the handler returns, throwing `operator new` calls `memory_halt_out_of_memory(size)`; it never returns nullptr. Nothrow new returns nullptr. The kernel installs no returning handler.
* **Re-entry guard (added in this amendment).** If the handler, or anything it calls, runs out of memory again, the guard halts the CPU instead of recursing into the handler. The guard is cleared when a handler returns normally, so a later OOM reaches the handler again. Both properties are tested in child processes (a missing guard shows up as stack-overflow SIGSEGV instead of the expected halt; both mutations were caught).
* The default halt path performs no allocation (it only writes VGA memory) so it cannot fail itself. It is separate from the M4 exception reporter.
* Host tests cannot execute "halt", so the host build traps (SIGILL); the kernel build executes `cli; hlt`. The test asserts the default path never returns.

### Audit of existing code
No kernel code used plain `new`/`delete` (only placement `new` inside `filesystem`/`block_cache`). `talonc` is the only direct `kmalloc` caller and handles nullptr. So the change in plain-`new` OOM behaviour (nullptr -> halt) affects no existing code path.

## M4 - Minimal IDT + page-fault handling

### Architecture
* `kernel/exception.h/.cpp` define packed 16-byte IDT gates and the 10-byte IDTR operand. The IDT is 16-byte aligned, contains gates for vectors 0..31, uses the boot GDT's 64-bit code selector `0x08`, and installs present ring-0 interrupt gates (`0x8E`).
* `exception_init()` loads the IDT with `lidt` from `kernel/entry.asm` after entering long mode and before global C++ constructors run. This protects both constructors and `kernel_main()` from the standard CPU exceptions.
* `kernel/entry.asm` supplies stubs for vectors 0..31. Stubs for exceptions with CPU error codes retain that code; all other stubs push a synthetic zero. Both forms then push the vector, giving a consistent frame. The common entry clears DF, saves all general-purpose registers, captures CR2, aligns the stack for the SysV C++ ABI and calls the non-returning dispatcher. A register restore and `iretq` path is present if dispatch ever returns.
* CPU error-code vectors handled by the stubs are 8, 10-14, 17, 21, 29 and 30. Optional newer exceptions only occur when supported by the processor; legacy exceptions use the standard x86 error-code set.

### Diagnostics and allocator interaction
* A page fault immediately captures CR2 in the common assembly entry. The C++ reporter writes directly to VGA memory and halts with interrupts disabled; it does not use streams, filesystem services, dynamic allocation, `kmalloc()` or `kfree()`.
* The page-fault screen shows the virtual address, raw CPU error code, and whether the cause was a protection violation or missing page, read or write, user or supervisor access, and instruction fetch when the CPU sets that bit.
* Other vectors use the default report with exception number, error code and instruction pointer, then halt. No exception returns to the faulting code.
* The kernel heap remains single-context and is not exception/interrupt safe. This restriction is now documented beside its API. No allocator API or behavior was changed.
* `pfaulttest` is a manually invoked shell command that writes to unmapped address `0x40000000`; it is not called during startup and intentionally halts after reporting.

### Verification
* `make test` - passed, including the new host test for IDT gate layout and page-fault error-bit decoding; existing TalonsFS, allocator, paging, shell-navigation and regression tests also passed.
* `make size-check` - passed with a real freestanding compile and link using `-m64 -ffreestanding ... -Wall -Wextra` and `linker.ld`.
* Undefined-symbol check: `nm -u build/kernel.bin` returned no symbols. The final ELF contains `exception_init`, `exception_dispatch`, the 32-entry `exception_stub_table`, and `exception_test_trigger_page_fault`.
* QEMU normal boot - passed; screen showed `TALONSOS KERNEL ONLINE`, `TALONS SYSTEM TESTS: VALID`, and the shell prompt.
* QEMU controlled fault - passed; after manually issuing `pfaulttest`, the screen showed address `0x40000000`, error `0x2`, and `not present, write, supervisor`, then halted.
* The 123-sector limit and TalonsFS start at block 128 were not changed.

### Limits after M4
| | After M3 | After M4 |
|---|---:|---:|
| Kernel image | 43,704 B / 86 sectors | 45,208 B / 89 sectors |
| Sectors left (limit 123) | 37 | **34** |
| Bytes left in kernel image budget | 18,944 | **17,408** |
| `_kernel_end` | 0x6E3C1 | 0x6EBC0 |
| Bytes left to 0x80000 | 72,767 | **70,720** |

### Remaining limitations
* No IRQ handlers, PIC/APIC setup, or `sti` were added; this milestone handles CPU exceptions only.
* The IDT uses the current kernel stack for exceptions. There is no TSS/IST alternate stack, so a stack-corruption fault or double fault may still be unrecoverable.
* Reports are VGA text-mode diagnostics and halt permanently; there is no serial logger or recovery path.
* QEMU verifies the normal boot and one controlled page fault. Other exception vectors, physical hardware, and allocator corruption/OOM runtime scenarios remain untested.
* The OOM path in `memory_fatal_out_of_memory()` remains a separate VGA halt implementation; it is not yet unified with the exception reporter.

## M5 - Paging and virtual memory

### Implementation
* `kernel/paging.cpp` keeps the boot PML4 and recursive slot 510. It validates
  the boot table links, canonical virtual addresses, physical-address width,
  page alignment, supported flags, and the reserved recursive PML4 slot.
* `mapPage()` now creates intermediate tables from the PMM, initializes them
  through the recursive mapping, checks every level for an existing mapping,
  and rolls back newly allocated tables if a later allocation fails. It never
  replaces a present 1 GiB, 2 MiB, or 4 KiB mapping.
* `unmapPage()` handles 1 GiB and 2 MiB leaves by splitting them down to 4 KiB
  entries, removes only the requested page, invalidates its TLB entry, and
  releases empty PMM-owned page tables. The original mapping’s physical
  addresses and leaf attributes are preserved across a split.
* Public mapping flags cover writable, user, write-through, cache-disable and
  no-execute. Ancestor writable/user bits permit the leaf policy to take
  effect; leaf entries carry the requested page permissions. NX mappings are
  accepted only when EFER.NXE is enabled. `entry.asm` enables NXE when CPUID
  reports support, without changing the bootloader layout.
* `queryPage()` reports the translated physical address, leaf flags and current
  page size. `translate()` now uses the same walk and supports 4 KiB, 2 MiB and
  1 GiB mappings.
* `mapTemporaryFrame()` provides one non-reentrant high-half scratch mapping.
  `allocateZeroedFrame()` and `mapNewPage()` use it to clear newly allocated
  frames. `mapNewPage()` transfers frame ownership to its caller; callers
  must unmap and release the frame explicitly.
* `invlpg` runs after mapping changes and recursive aliases are invalidated
  before newly allocated page tables are accessed. The M4 exception path
  remains allocation-free.
* Corrected recursive PDPT and page-directory virtual addresses in
  `paging_internal.h`; these formulas were off by one level and were exposed
  by the real high-half QEMU mapping smoke test.

### Tests and verification
* `test_paging` now links the real `paging.cpp` and PMM against a host page-table
  simulator. It covers canonical/alignment/flag checks, duplicates, 4 KiB
  mapping and translation, permissions, invalidation, allocator exhaustion,
  rollback after partial table allocation, page-table reclamation, 1 GiB and
  2 MiB splits, temporary mapping exclusivity and zero-on-allocation.
* `make test` - passed after the paging changes.
* `make size-check` - passed with a fresh freestanding compile and link.
  Kernel image: **47,160 bytes / 93 of 123 sectors**; remaining capacity:
  **30 sectors / 15,360 bytes**. `_kernel_end` is `0x6F360`, leaving
  **68,768 bytes** below `0x80000`.
* QEMU normal boot - passed. Kernel startup performed a real high-half
  `mapNewPage()` smoke test: checked the zero-filled frame, wrote and read a
  marker, verified the translation, then unmapped and released the frame. The
  shell reached its prompt and displayed `TALONS SYSTEM TESTS: VALID`.
* QEMU controlled fault - passed by issuing `pfaulttest`; the M4 handler
  reported address `0x40000000`, error `0x2`, `not present, write, supervisor`,
  then halted as expected.
* Stage 2 remained exactly 2,048 bytes; its kernel-sector parameter was 93,
  below the existing 123-sector cap. No bootloader layout or TalonsFS start
  block was changed.

### Limitations
* The subsystem is single-CPU and has no page-table locks or SMP TLB
  shootdown. Callers must serialize changes if concurrency is introduced.
* The temporary mapping window allows one outstanding mapping and must not be
  nested or used from exception/interrupt handlers.
* `mapPage()` maps caller-owned physical memory. `unmapPage()` deliberately
  does not release the mapped data frame; the caller owns that lifecycle.
* There is no demand paging, copy-on-write, user process address-space switch,
  or protection-fault recovery. The M4 page-fault handler remains diagnostic
  and halts.
* The PMM still tracks physical memory only below 1 GiB. No physical-hardware
  run was performed; runtime verification used QEMU.

## M6 - User-space memory foundation

### Architecture and address layout
* `AddressSpace` owns a distinct PML4 frame and a private clone of the lower
  half PDPT. The existing boot PML4/PML4 slot 510 recursive arrangement stays
  intact; each address space points its own slot 510 back to its own PML4.
* Virtual addresses `[0, 1 GiB)` retain the boot supervisor identity map. User
  mappings may occupy `[1 GiB, 512 GiB)` in PML4 slot 0. The scratch mapping
  window at PML4 slot 256 is reserved and is created privately when needed.
  Other kernel upper-half PML4 entries are copied as shared supervisor
  mappings. The recursive slot is never shared.
* The first PDPT entry and its directory remain shared with the kernel and
  supervisor-only. User page tables are created only in previously empty
  PDPT entries. Consequently the user bit at the PML4 does not grant access to
  the identity-mapped kernel: every level below it must also allow user access.
* No ring-3 entry, scheduler, process object, TSS privilege stack, demand
  paging, or fault recovery was introduced. Address-space switching is an
  explicit CR3 operation for kernel-side setup and testing; callers must not
  switch while holding pointers into another address space.

### Ownership and APIs
* `createAddressSpace()` allocates and zeroes a private PML4 and PDPT using the
  existing PMM and the M5 temporary frame window. It leaves the active CR3
  unchanged and rolls back partial allocation failures.
* `mapUserPage()` accepts only user-range addresses and user permission flags.
  It allocates and zeroes a PMM frame, then transfers ownership of the frame
  and any newly created page tables to the address space. It never accepts a
  caller-supplied physical address, preventing a user mapping from aliasing
  kernel or device memory. Duplicate mappings fail without replacing a leaf.
* `unmapUserPage()` removes a user leaf, invalidates it through the M5 paging
  layer, releases the owned data frame, and reclaims empty private page tables.
  `destroyAddressSpace()` releases remaining user frames, private page tables,
  cloned PDPT and PML4. It does not release the shared identity directory or
  copied kernel upper-half tables. Destroying the active space first switches
  back to the boot root.
* `queryAddressSpace()` temporarily selects the requested root and restores
  the prior root. `activateAddressSpace()` intentionally changes CR3 and must
  be paired by the caller with a deliberate later switch. The address-space
  handle is invalidated after successful destruction.
* Mappings are supervisor-only unless created through `mapUserPage()`.
  User pages can be read-only or writable; NX is enabled when the processor
  supports EFER.NXE, as in M5. Raw `mapPage()` refuses user mappings so frame
  ownership remains unambiguous. User pages are only backed by newly allocated
  PMM frames, which are zeroed before becoming visible.

### Fault policy and limitations
* The M4 page-fault handler remains allocation-free and never attempts demand
  allocation or recovery. It reports fault address, present/protection status,
  read/write, user/supervisor and instruction-fetch state. A user fault is
  labelled `USER PAGE FAULT`; unsupported faults halt, as does the existing
  kernel-fault path. The controlled `pfaulttest` remains unchanged.
* User bits are verified at every level by hardware. Kernel identity mappings
  remain supervisor-only even though the private PML4 entry points to a
  user-capable PDPT. Kernel upper-half mappings are inherited supervisor-only.
* This is a single-CPU foundation with no locks, process lifecycle, TSS/IST
  user stack, ring-3 transition, SMP TLB shootdown, copy-on-write, executable
  loader, or recoverable user exception context. Do not claim user-mode code
  execution; QEMU tests run at CPL0 while checking the page-table user/NX and
  writable bits. Physical memory above the PMM's 1 GiB limit remains unusable.

### M6 host tests and runtime verification
* `test_paging` adds independent-root creation and destruction, two-space
  isolation, retained supervisor kernel mapping, user/read-only/writable/NX
  flags, invalid range and alignment, duplicate mappings, explicit activation
  and restoration, active-space destruction, user unmapping, frame/table
  reclamation, failed root creation, and partial mapping allocation rollback.
* `kernel_main()` runs a controlled address-space check after the M5 high-half
  mapping test. It maps writable and read-only zeroed user pages, queries the
  user and supervisor mappings, activates the test root, reads/writes the
  writable page at CPL0, restores the boot root, destroys the space, and
  checks that PMM free-frame count returns to its baseline.
* `make test` - passed, including all existing filesystem, allocator, paging,
  exception and regression tests plus the expanded address-space tests. The
  host paging backend simulates page tables and physical frame contents; it
  does not execute ring 3 or exercise hardware privilege transitions.
* `make size-check` - passed with the real freestanding compile and link.
  `make all` also passed and produced the disk image; stage 2 remained exactly
  2,048 bytes and the kernel load count was 98 sectors.
* QEMU 10.2.2, 128 MiB guest - normal boot passed. Startup ran both the M5
  high-half map test and M6 address-space test, reached the shell, and showed
  `TALONS SYSTEM TESTS: VALID`. The M6 test activated a separate CR3, accessed
  a zeroed writable user page at CPL0, checked read-only/user/NX bits and the
  supervisor identity mapping, restored the boot CR3, destroyed the test
  address space and verified PMM frame-count recovery.
* QEMU `pfaulttest` - passed. The M4 handler still reported `0x40000000`,
  error `0x2`, `not present, write, supervisor`, then halted. A user-mode page
  fault was not runtime-tested because no ring-3 entry or TSS user stack exists.
* `git diff --check` - passed.

### M6 limits
The initial size estimate was 4-7 KiB. M6 added 3,048 bytes over the M5 image.
Actual freestanding link after the M6 implementation: **50,208 bytes / 99 of
123 sectors**, leaving **24 sectors / 12,288 bytes**. `_kernel_end` is
`0x6FF40`, leaving 65,728 bytes to the
`0x80000` bound. Stage 2 and the TalonsFS start block remain unchanged.

## M7 - Memory debugging and final OOM handling

### Audit and changes
M7 retained the existing PMM `verify()` and heap `check()` implementations,
the allocator counters, and the `meminfo`/`memmap` commands. It did not add a
second allocator or change paging/address-space ownership. The focused changes
were:

* `memory_check()` and `memory_get_stats()` no longer lazily initialize the
  heap. Before `memory_init()` they fail clearly and leave allocator state
  untouched. Both are read-only diagnostics.
* `memcheck` is a shell command that directly writes bounded diagnostic text
  to VGA memory. It invokes PMM and heap integrity checks without streams or
  dynamic allocation, and reports counters plus the first heap issue when
  available. `meminfo` and `memmap` remain available.
* Throwing scalar/array `operator new` now always halts if allocation fails,
  even when an installed fatal notification handler returns. Nothrow forms
  still return null. `memory_halt_out_of_memory()` has a fixed VGA/HLT kernel
  path and a host trap; the kernel path performs no allocation. `oomtest`
  invokes this path deliberately and halts the test VM.
* Kernel startup now includes PMM and heap integrity in the allocator smoke
  check. The exception reporter remains allocation-free; no allocator
  recovery was added to exception paths.

### Diagnostics and APIs
* `talons_frames::verify()` checks PMM bookkeeping without changing PMM
  counters or allocation state.
* `memory_check(talons_heap::CheckReport&)` validates heap metadata, canaries,
  poison and block relationships. It does not repair corrupted state.
* `memory_get_stats(talons_heap::Stats&)` provides heap counters without
  initializing it. A false result means statistics are unavailable (for
  example, the heap has not been initialized).
* Shell commands: `meminfo` reports the memory map and allocator statistics;
  `memmap` lists BIOS memory-map entries; `memcheck` runs both integrity
  checks. `oomtest` is an intentional fatal test and should only be run in a
  disposable VM. `pfaulttest` remains the controlled kernel-fault command.

### OOM and ownership limitations
`kmalloc` and PMM allocation continue to report failure through null/zero and
failure counters. Contiguous PMM allocation is all-or-nothing. Throwing `new`
cannot return a null pointer; it calls the fatal notification and then halts
if that callback returns. Nothrow `new` returns null. Placement new does not
allocate. There is no allocator recovery from detected heap corruption.

The PMM, heap, paging code and address-space APIs remain single-CPU and
unsynchronized. Callers must serialize access if concurrency is introduced.
No allocation is allowed from exception/interrupt contexts. Address-space
destruction retains M6's rule of switching away from an active space before
reclaiming it. The existing limits still apply: PMM tracks physical frames
below 1 GiB; the heap is fixed at 1 MiB; no Ring 3 execution, process teardown,
demand paging, scheduler integration, or recoverable user fault context is
implemented. QEMU does not substitute for physical-hardware verification.

### M7 tests and verification
* Added host coverage for read-only/non-initializing diagnostics, PMM state
  preservation after failed contiguous allocation, PMM verification state
  preservation, repeated allocate/free cycles, heap corruption reporting,
  and throwing `new`/`new[]` halting when a fatal callback returns.
* `make test` - passed, including the complete existing suite and all memory
  tests. The navigation regression fixture was regenerated by the test target.
* `make size-check` - passed using a real freestanding compile and link.
  Kernel image: **52,288 bytes / 103 of 123 sectors**, leaving **20 sectors /
  10,240 bytes**. `_kernel_end` is `0x70760`, leaving 63,648 bytes to `0x80000`.
* `make all` - passed; stage 2 remained 2,048 bytes, kernel load count is 103,
  and the TalonsFS start block remains 128.
* QEMU 10.2.2, 128 MiB guest - normal boot passed and showed
  `TALONS SYSTEM TESTS: VALID`. `meminfo` showed allocator statistics and OK
  self-checks; `memmap` listed the guest's seven BIOS memory-map entries.
  `memcheck` reported `PMM: OK` and `Heap: OK` with zero failures
  and corruption counters. `pfaulttest` halted with address `0x40000000`,
  error `0x2`, and `not present, write, supervisor`. `oomtest` displayed
  `KERNEL PANIC: OUT OF MEMORY` and halted. These are QEMU runtime checks; they
  do not test physical hardware or Ring 3 execution.
* `git diff --check` - passed.
