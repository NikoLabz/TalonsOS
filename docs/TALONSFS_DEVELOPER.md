# TalonsFS Developer Documentation

Status: describes the implementation as of Phase 3 completion, after
the audit and fixes documented in `AUDIT_REPORT.md`. Read that file
first for the history of what was broken and why; this file describes
the resulting contract going forward.

## 1. BlockDevice interface

`filesystem/talonsfs/include/talonsfs/block_device.h`:

```cpp
class BlockDevice {
public:
    virtual ~BlockDevice() = default;
    virtual bool readBlock(uint64_t blockNumber, void* buffer) = 0;
    virtual bool writeBlock(uint64_t blockNumber, const void* buffer) = 0;
    virtual uint64_t getBlockCount() const = 0;
    virtual uint32_t getBlockSize() const = 0;
};
```

Every implementation must:
- Transfer exactly `getBlockSize()` bytes on every read/write.
- Reject (`return false`) any `blockNumber >= getBlockCount()`.
- Reject a null buffer.
- Leave the target buffer/device untouched on failure -- callers are
  entitled to assume a `false` return did not partially write.

## 2. KernelBlockDevice

`kernel/block_device.h` / `.cpp`. The only ATA PIO implementation.
Translates filesystem-relative block numbers to physical disk blocks
by adding `FILESYSTEM_START` (128) -- this is the **only** place that
translation happens; everything above this layer works exclusively in
filesystem-relative numbers. `getBlockCount()` returns
`TOTAL_DISK_BLOCKS - FILESYSTEM_START` (32640). `getBlockSize()`
always returns 512.

## 3. BlockCache

`kernel/block_cache.h` / `.cpp`. Sits directly above
`KernelBlockDevice` in `kernel/filesystem.cpp`:

```cpp
KernelBlockDevice physicalDevice;
BlockCache device(physicalDevice);
```

32 entries, 512 bytes each, write-back, dirty-tracked.

**Construction and the freestanding constructor hazard.** `BlockCache`
has a non-trivial constructor (it zeroes every entry) and a reference
member, so it cannot be constant-initialized and requires a global
C++ constructor to run before first use. This was the root cause of
the Phase 3 boot crash documented in `AUDIT_REPORT.md`: the kernel is
linked with `ld` directly, with no crt0/crti/crtbegin, so nothing
calls global constructors automatically. `kernel/entry.asm` now walks
`[__init_array_start, __init_array_end)` explicitly before calling
`kernel_main`. **Any future global object with a non-trivial
constructor depends on this loop still running before it is first
used.** If you add a new global object of non-POD type, verify
`.init_array` still contains an entry for it
(`objdump -s -j .init_array build/kernel.bin` after building) and
that entry.asm's ctor loop still precedes any code path that touches
it.

**Replacement policy.** `selectEntry()` prefers a never-used (`!valid`)
slot before evicting any valid entry, so filling an empty cache never
triggers a write-back. Once every slot is valid, eviction is plain
round-robin by entry index -- not LRU, not by write recency.

**Block-size guard.** The constructor checks
`device.getBlockSize() == CACHE_BLOCK_SIZE` (512) and sets an internal
`usable` flag. If false, every `readBlock`/`writeBlock` call fails
immediately. This exists because `CacheEntry::data` is a fixed
512-byte array: without the guard, a device reporting any other block
size would cause `readBlock`/`writeBlock` to copy the *device's*
block size into/out of that fixed array, either overflowing into the
next `CacheEntry` in the array (device block > 512) or silently
delivering a partially-stale buffer to the caller (device block <
512). Call `isUsable()` if you need to check this explicitly; in
practice the kernel only ever constructs a `BlockCache` over a
512-byte `KernelBlockDevice`, so this should never trip in production,
but it is what stands between a configuration mistake and silent
memory corruption.

**Flush and invalidate.** `flush()` writes every dirty entry through
to the underlying device and clears the dirty flag on success;
`invalidate()` calls `flush()` first and only clears/marks entries
invalid if the flush succeeded -- a failed flush leaves the cache
exactly as it was, so dirty data is never silently discarded.

**Who must call flush(), and when.** The cache does **not** flush
itself on any schedule, timer, or destructor (kernel objects are
never destroyed). Every code path that ends a session with the
filesystem must flush explicitly:
- `filesystem_unmount()` calls `flushMetadata()` (writes into the
  cache) **and then** `device.flush()` (drains the cache to the
  physical device). Both must succeed or unmount reports failure and
  leaves the filesystem mounted.
- `filesystem_format()` flushes twice: once after writing metadata,
  once after writing the superblock. See "Format ordering" below for
  why this is two separate flushes and not one.
- The shell's reboot handler (`Byebye!` in `kernel/shell.cpp`) calls
  `filesystem_unmount()` before pulsing the reset line, specifically
  so a reboot cannot discard dirty write-back data. If you add a new
  code path that can end the kernel session (shutdown, panic, a
  future `Ctrl-Alt-Del` handler, etc.), it must do the same or risk
  silent data loss on a write-back cache.

**Format ordering and the write-back reordering hazard.**
`filesystem_format()`'s original design intent, stated in its own
comment, is to write the superblock last so a half-formatted volume
never looks valid. With a write-back cache, "written last" by the CPU
is not the same as "reaches the disk last": round-robin eviction
follows entry index, not write order, so without an explicit barrier
the superblock could physically reach the platter before some
metadata block still sitting dirty in another cache entry. This was
verified experimentally during the Phase 3 audit (see
`AUDIT_REPORT.md`) -- a synthetic eviction-pressure test showed the
superblock landing on the mock device before metadata that was
written earlier. The fix is the two `device.flush()` calls described
above: one drains all metadata before the superblock write begins,
the second confirms the superblock itself is committed before
`filesystem_format()` reports success. If you touch the format path,
preserve both barriers.

## 4. Two directory implementations

There are two independent implementations of TalonsFS directory-entry
storage, not one:

- `kernel/filesystem.cpp`'s static functions `appendDirectoryEntry`,
  `findDirectoryEntry`, `removeDirectoryEntry` -- hand-written, no
  STL, linked into the kernel.
- `filesystem/talonsfs/src/directory.cpp`'s `Directory` class,
  wrapped by `filesystem/talonsfs/src/filesystem.cpp`'s `FileSystem`
  class -- uses `<vector>`/`<algorithm>`/`<limits>`, explicitly
  marked host-only, linked into `tools/mkTalonsFS` and several host
  tests (`test_persistence`, `test_formatter`, `test_directory`,
  `test_directory_list`).

This is **not** accidental duplication to be cleaned up. It exists
because the kernel is freestanding and genuinely cannot link the host
implementation (`directory.cpp` uses the STL; the kernel build has no
libstdc++). Both implementations must independently agree on the same
on-disk `DirectoryEntry` layout (`directory_entry.h`, shared by both),
and both are exercised by real tests, so neither can be deleted
without losing real coverage: `mkTalonsFS` (which produces every test
fixture disk image, including the one `test_navigation_regression`
boots against) depends on `FileSystem`/`Directory`; the kernel image
that actually ships depends on `kernel/filesystem.cpp`'s own logic.

**Practical consequence for anyone changing the directory-entry
format:** a change to `DirectoryEntry` or to directory-entry semantics
must be made in both places, and the two are not made to run the same
regression tests against each other. There is currently no
cross-check that they behave identically beyond both passing their
own, separate test suites.

The rest of the TalonsFS core (bitmap, block_allocator, node_allocator,
node_manager, node_data_manager, data_block_manager, layout,
superblock checksum) genuinely is shared -- the identical `.cpp` files
are compiled into both the kernel and the host tools, listed in both
`KERNEL_SRCS`/kernel's `TALONSFS_SRCS` and `TOOL_SRCS` in the
Makefile.

**The superblock checksum function is duplicated, not shared, despite
being identical.** `superblock_checksum.cpp` (linked into the kernel)
and `superblock_utils.cpp` (linked into host tools, which also
provides `validateSuperblock`/`readSuperblock`, not needed by the
kernel) both define `TalonsFS::calculateSuperblockChecksum` with
byte-for-byte identical logic today. Linking both into the same
binary is a duplicate-symbol link error by construction (the Makefile
never does this), but nothing prevents the two copies from drifting
out of sync if one is edited and the other is not.

## 5. Filesystem lifecycle

```
filesystem_format()  --calls-->  filesystem_init()  (reload-through-mount)
filesystem_init()    mount
filesystem_unmount()  unmount, flush cache
```

`filesystem_init()` populates a set of file-scope static buffers and
pointers in `kernel/filesystem.cpp` (there is exactly one mounted
filesystem at a time; there is no concept of multiple simultaneous
mounts). Metadata (node table, block bitmap, node bitmap) is read
once at mount time into static arrays sized for the maximum capacity
the kernel budgets for (`NODE_TABLE_CAPACITY_BLOCKS`,
`BLOCK_BITMAP_CAPACITY_BLOCKS`, `NODE_BITMAP_CAPACITY_BLOCKS`) and
kept resident in memory for the whole mount session; every metadata
mutation writes through to those buffers immediately and is flushed
to the cache with `flushMetadata()` at the end of the relevant
operation.

## 6. Filesystem APIs and error handling

Public entry points are declared in `kernel/filesystem.h`. Every
public function follows the same pattern: reset
`lastFilesystemError` to `None` on entry, validate arguments and mount
state, perform the operation, call `setFilesystemError()` on any
failure path, return `bool`. `filesystem_get_last_error()` and the new
`filesystem_error_string()` (added in this audit pass) let callers
retrieve and print the specific cause.

**Rollback and error preservation.** Several operations perform a
partial action and then roll it back on later failure (e.g.
`filesystem_copy` creates the destination file, then reads the
source, then writes; if the read or write fails it removes the
destination it just created). Rolling back by calling another public
`filesystem_*` function is dangerous, because every public function
resets `lastFilesystemError` to `None` on entry -- calling
`filesystem_remove()` as part of a rollback will silently erase the
error you were about to report. This was a real bug found and fixed
in this audit: `filesystem_copy`'s rollback paths now capture
`lastFilesystemError` into a local before calling
`filesystem_remove()`, then restore it afterward. If you write a new
rollback path that calls another public filesystem function, you must
do the same.

**`DirectoryNotEmpty`.** Added in this audit. Previously, removing a
non-empty directory reported `NotADirectory` -- literally the
opposite of the truth (the target *is* a directory; it just isn't
empty). Any code checking for `NotADirectory` to detect this case
must be updated to check `DirectoryNotEmpty` instead.

## 7. Permissions -- storage without enforcement

`filesystem/talonsfs/src/permissions.cpp` (bit layout, semantics,
and accessor functions) is fully implemented and unit-tested (13/13
passing in isolation). It is **not linked into the kernel** --
`permissions.cpp` does not appear in `KERNEL_SRCS` or the kernel's
`TALONSFS_SRCS` in the Makefile -- and `kernel/filesystem.cpp` never
calls any of `hasReadPermission`/`hasWritePermission`/
`hasExecutePermission`. Permission bits are set correctly at node
creation and survive persistence correctly; nothing currently
consults them before allowing an operation.

This audit deliberately did not add enforcement. `Node` has no
owner/UID field, and TalonsOS has no multi-user identity model, so
there is no answer to "permission for whom?" anywhere in the current
design. Adding enforcement without first answering that question
would mean inventing semantics that aren't specified. If enforcement
is wanted, the identity model needs to be designed first (even a
trivial single-implicit-user model would need an explicit decision
about what "checking permission" means when there is only ever one
possible caller).

## 8. Testing

`make test` builds and runs, in order: `test_integrity`,
`test_permissions`, `test_filesystem`, `test_block_cache`,
`test_block_cache_full`, `test_rtc`, `test_navigation_regression`,
then a loop over 21 further unit tests covering bitmap, both
allocators (including double-free/out-of-range protection), the node
table, directory entries, layout, superblock validation/magic, the
formatter, file/persistence round-trips, and RTC field conversion
validated against the host's own `timegm()` across 33,600 dates.

`test_navigation_regression.cpp` is the highest-value test in the
suite: it links the **real, unmodified**
`kernel/filesystem.cpp`/`terminal.cpp`/`shell.cpp`/`block_cache.cpp`
-- exactly what ships in the kernel image -- against host test
doubles for ATA, VGA, and keyboard I/O (`tests/host_stubs/`), and
drives them through `shell_handle_line()` exactly as the real shell
loop would. Its fixture disk (`build/test_disk.img`) is produced by
the real `mkTalonsFS`, so the fixture matches production formatting.
Because it mutates its fixture in place, `make test`'s recipe deletes
and regenerates that fixture on every invocation rather than relying
on Make's file-presence tracking -- omitting this was found, during
this audit, to make a second consecutive `make test` run (without an
intervening `make clean`) report false failures from a dirty leftover
fixture.

**To add a new kernel-level regression test**, follow this file's
pattern rather than writing a standalone host-only test when the
behavior you're testing lives in kernel code: link the real
`kernel/*.cpp` files against `tests/host_stubs/`, not a reimplemented
mock of kernel behavior. A mock can drift from what the kernel
actually does; the stubs only replace hardware I/O.

**To add a new host-only TalonsFS-core test**, add
`filesystem/talonsfs/src/test_yourname.cpp` and it is picked up
automatically by the Makefile's `$(BUILD)/test_%` pattern rule and
the `TEST_TALONSFS_UNIT_NAMES` list -- add your test's base name to
that list and it will build and run under `make test`.

## 9. Common failure modes to watch for

- **Adding a global object with a non-trivial constructor** and
  forgetting it depends on `entry.asm`'s `.init_array` loop still
  running before first use (see section 3). Symptom: a triple fault
  the first time a virtual method is called on the object, with
  `CR2`/`RIP` pointing into low memory (the zeroed vtable pointer
  dereferences into the real-mode IVT).
- **Calling a public `filesystem_*` function as part of a rollback**
  inside another `filesystem_*` function, without preserving
  `lastFilesystemError` first (see section 6).
- **Bypassing `BlockCache::flush()`** on any new code path that ends
  a filesystem session (see section 3, "Who must call flush()").
- **Changing `DirectoryEntry` or directory-entry semantics** in only
  one of the two directory implementations (see section 4).
- **Assuming block growth can relocate a node's data** -- it cannot;
  `NodeDataManager::allocateBlock()` requires the immediately
  following block to be free and fails outright otherwise (see
  `docs/TALONSFS_DISK_FORMAT.md` section 8). A sufficiently fragmented
  data region can make a node unable to grow even with free space
  elsewhere on the volume.
