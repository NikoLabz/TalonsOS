# TalonsFS Disk Format

Status: describes the implementation as of Phase 3 completion.
Authoritative source: `filesystem/talonsfs/include/talonsfs/*.h`,
`filesystem/talonsfs/src/layout.cpp`, `kernel/block_device.h`.
All struct sizes below were confirmed by running `sizeof()` in the
host test suite (`test_node.cpp`, `test_superblock.cpp`,
`test_directory_entry.cpp`), not computed by hand, so they include
whatever padding the compiler actually inserts.

## 1. Physical disk layout (whole disk, as booted)

```
physical sector 0            bootloader (boot/boot.asm)
physical sectors 1-127       kernel.bin flat image (max 127 sectors,
                              enforced at assemble time by
                              boot/boot.asm's %error check)
physical sector 128 onward   TalonsFS volume
physical sector 32768        end of disk (TOTAL_DISK_BLOCKS)
```

`FILESYSTEM_START = 128` and `TOTAL_DISK_BLOCKS = 32768` are defined
once, in `kernel/block_device.h`, and used nowhere else -- the
Makefile's `DISK_IMG_BYTES` and `tools/mkTalonsFS` are both driven
from this same pair of constants so the boundary can never drift out
of sync between the two.

`KernelBlockDevice` (`kernel/block_device.cpp`) is the only place the
`+ FILESYSTEM_START` translation happens: every block number TalonsFS
itself works with, at every layer above `KernelBlockDevice`, is
**filesystem-relative** (block 0 = the first block of the volume =
physical block 128).

Filesystem-visible block count:
`TOTAL_DISK_BLOCKS - FILESYSTEM_START = 32768 - 128 = 32640` blocks.

## 2. Block size

**512 bytes**, fixed, for every kernel-facing component
(`KernelBlockDevice::getBlockSize()`, `BlockCache`'s hard-coded
`CACHE_BLOCK_SIZE`, `TALONSFS_BLOCK_SIZE` in `constants.h`). The core
library (`calculateLayout()`, `Bitmap`, `NodeTable`, ...) is generic
over block size and the host test suite exercises other sizes (4096
is common in tests), but a real TalonsOS volume is always 512.

## 3. Filesystem-relative layout

Computed by `TalonsFS::calculateLayout()` (`layout.cpp`) from
`totalBlocks` and `blockSize`:

```
block 0                     unused (block 0 is never allocated by
                             calculateLayout; kept as a reserved/
                             boot-adjacent block)
block 1                     superblock (1 block, fixed)
block 2 .. 2+N-1            block bitmap
                               N = ceil(ceil(totalBlocks/8) / blockSize)
block (2+N) .. +M-1         node bitmap
                               M = ceil(nodeCount / (blockSize*8))
block (2+N+M) .. +T-1       node table
                               T = ceil(nodeCount * sizeof(Node) / blockSize)
block (2+N+M+T) onward      data region (dataStart)
```

`nodeCount` is derived from total volume size, not a fixed constant:
`nodeCount = (totalBlocks * blockSize) / 16384` (one node per 16 KiB
of volume). For the real TalonsOS volume (32640 blocks x 512 bytes =
16,711,680 bytes), this gives `nodeCount = 1020`.

Every one of these derived fields is also stored explicitly in the
superblock (see below) and is **re-derived and cross-checked** on
every mount: `kernel/filesystem.cpp`'s `filesystem_init()` calls
`calculateLayout()` again from the superblock's own `totalBlocks`/
`blockSize` and rejects the mount (`CorruptFilesystem`) if any stored
field disagrees with the recomputed one.

## 4. Superblock (block 1)

Confirmed size: **128 bytes** (of which `sizeof` fields below account
for less -- the remainder is compiler-inserted padding between the
`uint32_t` fields and their `uint64_t` neighbours).

| Field | Type | Meaning |
|---|---|---|
| `magic` | `uint64_t` | `0x54414C4F4E534653` -- ASCII `"TALONSFS"` |
| `version` | `uint32_t` | `1` (`TALONSFS_VERSION`) |
| `blockSize` | `uint32_t` | `512` for a real volume |
| `totalBlocks` | `uint64_t` | Filesystem-relative block count |
| `blockBitmapStart` | `uint64_t` | First block of the block bitmap |
| `blockBitmapBlocks` | `uint64_t` | Block-bitmap region size |
| `nodeBitmapStart` | `uint64_t` | First block of the node bitmap |
| `nodeBitmapBlocks` | `uint64_t` | Node-bitmap region size |
| `nodeTableStart` | `uint64_t` | First block of the node table |
| `nodeTableBlocks` | `uint64_t` | Node-table region size |
| `nodeCount` | `uint64_t` | Number of node slots |
| `dataStart` | `uint64_t` | First block available for file/directory data |
| `rootNode` | `uint64_t` | Always `0` |
| `freeBlocks` | `uint64_t` | Advisory only -- not consulted by mount or allocation |
| `freeNodes` | `uint64_t` | Advisory only -- not consulted by mount or allocation |
| `filesystemID` | `uint64_t` | Currently always set equal to `magic` |
| `checksum` | `uint64_t` | FNV-1a-64 over every other byte of the struct |

Checksum algorithm (`calculateSuperblockChecksum`, duplicated
byte-for-byte in `superblock_checksum.cpp` for the kernel and
`superblock_utils.cpp` for the host -- see
`docs/TALONSFS_DEVELOPER.md`):

```
checksum = 1469598103934665603        (FNV offset basis)
for each byte b in the struct, in declaration order,
    skipping the 8 bytes occupied by `checksum` itself:
    checksum ^= b
    checksum *= 1099511628211          (FNV prime)
```

The host-side `validateSuperblock()` checks this against the stored
`checksum` field as its final check, after validating that every
region fits within `totalBlocks` and doesn't overlap the next region.
The kernel's own mount path does not call `validateSuperblock()` at
all (it does not link `superblock_utils.cpp`); it performs the
equivalent per-field checks by hand against a freshly recomputed
`FilesystemLayout`, and does not appear to compare the checksum field
at all during mount -- only the individually-listed geometry fields.

## 5. Block bitmap

One bit per filesystem-relative block, LSB-first within each byte
(`Bitmap::get/set/clear`: `byte = index/8`, `bit = index%8`). Blocks
`0` through `dataStart - 1` (every metadata block, including block 0)
are marked allocated at format time and are never touched by
`BlockAllocator::free()`, which refuses any index below `dataStart`.

## 6. Node bitmap

One bit per node index, same LSB-first convention as the block
bitmap. Bit 0 (`TalonsFS::RootNode`) is set as a side effect of
creating the root directory through `NodeManager::create()` during
formatting.

## 7. Node table

A flat array of fixed-size `Node` records, indexed directly by node
number (`nodeTableStart + floor(index * sizeof(Node) / blockSize)`
gives the containing block). Confirmed size: **80 bytes** per node
(`node.h`'s declared fields plus padding: `type`, `flags`,
`reserved`, `size`, `createdTime`, `modifiedTime`, `dataStart`,
`dataBlocks`, and a 32-byte `reservedSpace` pad for future fields
without a format version bump).

## 8. Data region

Starts at `dataStart`, runs to `totalBlocks - 1`. A file or
directory's data occupies `dataBlocks` **contiguous** blocks starting
at `dataStart` (the node's own `dataStart` field, not to be confused
with the layout's `dataStart` -- naming collision between the two,
be careful reading the source). There is no indirect-block or extent
scheme: a node's entire allocation is one contiguous run.

Growing a node (`NodeDataManager::allocateBlock()`) does not search
for a non-contiguous block if the run cannot be extended -- it calls
`BlockAllocator::allocateSpecific(dataStart + dataBlocks)`, which
requires that exact next block to be free, and **fails outright**
(no allocation happens, growth simply does not proceed) if it is
already taken by something else. There is no fallback to relocate the
node's data elsewhere and no defragmentation. In a sufficiently
fragmented data region this means a node can fail to grow even while
free blocks exist elsewhere on the volume -- see
`docs/TALONSFS_DEVELOPER.md` for the practical consequence.

Directory data blocks hold a packed array of `DirectoryEntry` records
(confirmed size: **80 bytes** each):

| Field | Type | Meaning |
|---|---|---|
| `node` | `uint64_t` | Index into the node table |
| `type` | `uint8_t` | `NodeType` as a raw byte (1=File, 2=Directory) |
| `name` | `char[64]` | Null-terminated, unused bytes zero-filled |

A directory's `Node::size` is always an exact multiple of 80
(`sizeof(DirectoryEntry)`); `size / 80` is the entry count. This
invariant is checked on every directory read
(`CorruptFilesystem` if violated).

## 9. Reserved / unused regions

- Filesystem-relative block 0 is allocated at format time but never
  assigned any defined content by the current implementation. It
  exists as a spacer, not a reserved header block with defined
  fields.
- `Node::reservedSpace[32]` is zero-filled at creation and never
  written or read by any current code path -- reserved for future
  node fields without incrementing `TALONSFS_VERSION`.
- `Superblock::freeBlocks` / `freeNodes` are written at format time
  but not recomputed or consulted afterward by any code this audit
  found -- treat them as informational only, not authoritative free
  counts.

## 10. Version field

`TALONSFS_VERSION = 1`. The mount path rejects any other value
outright (`InvalidFilesystem`); there is no forward- or
backward-compatibility handling for a hypothetical version 2.
