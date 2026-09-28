# TalonsFS Specification

Status: describes the implementation as of Phase 3 completion.
Authoritative source: `filesystem/talonsfs/` and `kernel/filesystem.cpp`.

## 1. Purpose

TalonsFS is the native filesystem of TalonsOS. It is a small, flat,
single-volume filesystem with fixed-size metadata regions computed
from the size of the underlying block device. It has no journal, no
extents, no symbolic links, and no multi-volume support. It is
designed to be simple enough to implement correctly in freestanding
C++ (no libc, no libstdc++, no heap in the general case) while still
supporting directories, files, permissions, timestamps, structured
error reporting, and crash-safe formatting.

## 2. Architecture layers

TalonsFS exists in two forms that must agree on the on-disk format
but are separate pieces of code, because one runs in a freestanding
kernel and the other runs in hosted C++ tools and tests:

```
Kernel (freestanding, ships in the OS image)
  kernel/filesystem.cpp
    - mount / unmount / format / recover
    - file and directory operations
    - directory-entry storage: hand-written, no STL
      (appendDirectoryEntry / findDirectoryEntry / removeDirectoryEntry)
    - uses only the freestanding-safe TalonsFS core:
      node_table, bitmap, block_allocator, node_allocator,
      node_manager, node_data_manager, data_block_manager, layout,
      superblock checksum

Host (hosted C++, tools and tests only -- never linked into the kernel)
  filesystem/talonsfs/src/filesystem.cpp   the FileSystem class
  filesystem/talonsfs/src/directory.cpp    the Directory class
    - used by tools/mkTalonsFS.cpp (formats real disk images)
    - used by test_persistence, test_formatter, test_directory,
      test_directory_list
    - uses <vector>/<algorithm>/<limits>, explicitly marked
      "host-only" in each file's header comment
```

`FileSystem` is a high-level convenience API built on top of
`Directory`, `NodeManager`, and `NodeDataManager`. `Directory` is the
low-level primitive that reads and writes directory entries inside a
directory node's data blocks. Both exist **only** because the kernel
cannot link C++ standard library code; the kernel's own directory
logic in `kernel/filesystem.cpp` is a separate, hand-written
implementation of the same on-disk directory-entry format. See
`docs/TALONSFS_DEVELOPER.md` section "Two directory implementations"
for the consequences of this split.

The rest of the core (bitmaps, allocators, node table, layout,
superblock checksum) is genuinely shared: the same `.cpp` files are
compiled into both the kernel and the host tools, because none of
them use anything beyond freestanding-safe C++.

## 3. Blocks

- Block size is fixed at **512 bytes** for the kernel's disk (see
  `constants.h`: `TALONSFS_BLOCK_SIZE`). The core library itself is
  block-size-agnostic (`calculateLayout()` takes `blockSize` as a
  parameter and the host test suite exercises other sizes, e.g.
  4096), but every kernel-facing component (`BlockCache`,
  `KernelBlockDevice`, `kernel/filesystem.cpp`) hard-codes 512.
- All block numbers TalonsFS itself works with are **filesystem-
  relative**: block 0 is the first block of the TalonsFS volume, not
  the first block of the physical disk. See the disk-format document
  for how filesystem-relative numbers map to physical disk blocks.

## 4. Superblock

One block, at filesystem-relative block 1. Holds the magic number,
version, geometry of every metadata region, the root node index, free
counts, and a checksum. See `docs/TALONSFS_DISK_FORMAT.md` for the
exact field layout and offsets.

Validation happens differently in the two layers:
- The kernel (`filesystem_init()` in `kernel/filesystem.cpp`) checks
  every field individually against a freshly recomputed
  `FilesystemLayout` and rejects the mount on any mismatch, without
  calling a shared "validate" function.
- The host layer has a single `validateSuperblock()` function
  (`superblock_utils.cpp`) that performs the equivalent checks plus
  the checksum comparison, used by `readSuperblock()`.

Both paths must be kept consistent by hand if the superblock format
changes; there is no single shared validator.

## 5. Nodes

A `Node` (see `node.h`) is TalonsFS's inode equivalent: type
(Free/File/Directory), permission flags, size, created/modified
timestamps, and a pointer to the first data block plus block count.
Nodes are stored in a flat, fixed-size node table computed at format
time from the volume size (`bytesPerNode = 16 KiB`, i.e. roughly one
node per 16 KiB of volume). A parallel node bitmap tracks which node
indices are in use. Node 0 (`TalonsFS::RootNode`) is always the root
directory.

Nodes do **not** carry an owner or user ID. TalonsOS has no
multi-user identity model (see section 8, Permissions).

## 6. Directories

A directory is a `Node` of type `Directory` whose data blocks hold a
flat array of fixed-size `DirectoryEntry` records (node index, type
byte, 64-byte null-terminated name). There is no B-tree, hash table,
or other index: lookups are a linear scan, removal moves the last
entry into the removed slot's position to keep entries packed. This
is adequate for TalonsOS's current scale (fixed small directory
counts) and is not intended to scale to large directories without
redesign.

`..` (parent) traversal is **not** stored on disk. The kernel
maintains an in-memory directory stack (`kernel/filesystem.cpp`,
`directoryStack[16]`) for the *current* shell session's navigation
history; there is no persistent parent pointer in the directory-entry
format itself.

## 7. Files

A file is a `Node` of type `File`. Its data occupies a contiguous run
of `dataBlocks` blocks starting at `dataStart` (contiguous allocation,
not an extent list or indirect-block scheme -- see the disk-format
document for the allocation rule this implies). `filesystem_write_file`
always frees any existing blocks and reallocates fresh ones sized to
the new content; there is no in-place partial overwrite or append
without a full rewrite.

## 8. Permissions

Permission bits (read/write/execute, plus an
"explicitly initialized" bit) are stored in `Node::flags` and
implemented in `filesystem/talonsfs/src/permissions.cpp` /
`permissions.h`. This code is fully implemented and unit-tested in
isolation (13 tests, all passing).

**It is not linked into the kernel and not enforced by any
filesystem operation.** `permissions.cpp` does not appear in
`KERNEL_SRCS` or the kernel's `TALONSFS_SRCS` in the Makefile, and
`kernel/filesystem.cpp` never calls `hasReadPermission`,
`hasWritePermission`, or `hasExecutePermission`. Permission bits are
set correctly on node creation and persist correctly across
mount/unmount, but nothing currently consults them before allowing a
read, write, or directory operation to proceed.

This is a genuine gap, not an oversight in this audit: `Node` has no
owner/UID field, and TalonsOS has no multi-user identity concept
(single-user, ring-0, in the spirit of its TempleOS lineage). Before
enforcement could be added, a design decision is needed on what
identity a permission check is evaluated against -- there is
currently no answer to "permission for whom?" anywhere in the
codebase. Adding enforcement without that answer would mean
inventing semantics the specification does not define, which this
audit deliberately did not do.

## 9. Timestamps

`Node::createdTime` and `Node::modifiedTime` are Unix timestamps
(seconds since 1970-01-01 UTC), set by `NodeManager::create()` and
`NodeManager::write()` via `TalonsFS::currentUnixTime()`. Two
implementations exist, exactly one linked per binary:
- `kernel/kernel_time.cpp`: reads the CMOS RTC (ports 0x70/0x71),
  handles BCD/binary and 12/24-hour modes, and returns 0 if the clock
  cannot be read stably within a bounded number of attempts.
- `filesystem/talonsfs/src/time.cpp`: host-side, uses `<ctime>`.

A timestamp value of 0 means "unset/unknown" by convention, not
1970-01-01 -- callers should not treat 0 as a real time.

## 10. Allocation

- **Blocks**: `BlockAllocator` does a linear scan of the block bitmap
  starting at `dataStart`, returning the first free block. `free()`
  refuses to touch anything below `dataStart` (metadata blocks are
  never freed through this path).
- **Nodes**: `NodeAllocator` does a linear scan of the node bitmap for
  the first clear bit.

Neither allocator does best-fit, buddy allocation, or defragmentation.
Freed blocks/nodes are reused on the next linear scan that reaches
them.

## 11. Caching

`BlockCache` (`kernel/block_cache.h`/`.cpp`) sits between
`kernel/filesystem.cpp` and `KernelBlockDevice`. 32 entries, 512
bytes each, write-back, dirty tracking, round-robin replacement among
in-use entries (free entries are always preferred first). See
`docs/TALONSFS_DEVELOPER.md` for the full lifecycle contract
(flush/invalidate semantics, failure behavior, and why the cache
fails closed on a block-size mismatch rather than corrupting memory).

## 12. Mounting

`filesystem_init()`: reads and validates the superblock, recomputes
the expected layout and checks the superblock agrees field-by-field,
loads the node bitmap / block bitmap / node table into static
buffers, constructs the allocators and managers in place with
placement `new`, verifies the root node is a directory, then runs
orphaned-block recovery (see section 14) before marking the
filesystem mounted.

## 13. Unmounting

`filesystem_unmount()`: writes the in-memory node table / block
bitmap / node bitmap buffers back through `BlockCache` via
`flushMetadata()`, then calls `device.flush()` to force every dirty
cache entry to the physical device, then clears mount state. Both
steps must succeed or the unmount reports `DeviceError` /
`MetadataError` and the filesystem is **not** marked unmounted.

## 14. Recovery

On every mount, `recoverOrphanedBlocks()` walks every allocated node,
builds a bitmap of blocks actually claimed by reachable nodes, and
clears any block marked allocated in the on-disk block bitmap that no
node claims. This recovers from a crash between allocating a block
and recording it against a node, at the cost of a full node-table
scan on every mount. It does not detect or repair corrupted node
metadata itself -- that is a `CorruptFilesystem` mount failure, not a
silent repair.

## 15. Errors

See `docs/TALONSFS_DEVELOPER.md` for the full `FilesystemError` enum
and the error-propagation guarantees that were audited and fixed in
this pass (missing-file, directory-not-empty, and copy-rollback error
preservation).

## 16. Formatting

`filesystem_format()`: validates geometry fits the kernel's fixed
metadata buffer capacities, zeroes the metadata buffers, creates the
root directory node (via the normal `NodeManager::create()` path, so
it is correctly marked allocated), builds and writes the superblock
**last**, with explicit cache-flush barriers before and after the
superblock write so a write-back cache cannot reorder the superblock
ahead of the metadata it describes (see
`docs/TALONSFS_DEVELOPER.md`, "Format ordering"). Formatting then
reloads through the normal mount path, so a successful format has
also proven the volume mounts.
