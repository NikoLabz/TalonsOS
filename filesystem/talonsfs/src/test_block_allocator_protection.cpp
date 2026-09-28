// Guards BlockAllocator against operating below dataStart_ (the
// metadata region) and against double-allocation/double-free of the
// same block. The original version of this test only printed values
// for a human to eyeball and always exited 0 regardless of their
// correctness -- it could not actually catch a regression. Rewritten
// with real assertions, keeping the same scenarios.

#include <iostream>
#include <cstdint>

#include "../include/talonsfs/bitmap.h"
#include "../include/talonsfs/block_allocator.h"

namespace
{
    int failures = 0;

    void check(bool condition, const char* what)
    {
        std::cout << what << ": " << (condition ? "PASSED" : "FAILED") << '\n';
        if (!condition)
        {
            ++failures;
        }
    }
}

int main()
{
    constexpr uint64_t totalBlocks = 32;
    constexpr uint64_t dataStart = 20;

    uint8_t bitmapData[4] = {};

    TalonsFS::Bitmap bitmap(
        bitmapData,
        totalBlocks
    );

    TalonsFS::BlockAllocator allocator(
        bitmap,
        totalBlocks,
        dataStart
    );

    const int64_t block = allocator.allocate();

    check(block == static_cast<int64_t>(dataStart),
          "First allocation returns dataStart");

    // free() below dataStart_ must be a no-op: those blocks belong to
    // filesystem metadata (superblock, bitmaps, node table), not to
    // the data-block pool this allocator manages. If it touched them,
    // a later allocate() could hand out a metadata block as if it
    // were free data, corrupting the filesystem.
    allocator.free(5);

    check(bitmap.get(5) == false,
          "free() below dataStart_ is refused (bit 5 unaffected)");

    allocator.free(19);

    check(bitmap.get(19) == false,
          "free() at dataStart_ - 1 is refused (bit 19 unaffected)");

    // allocateSpecific() below dataStart_ must also be refused, for
    // the same reason.
    check(!allocator.allocateSpecific(10),
          "allocateSpecific() below dataStart_ is refused");

    check(bitmap.get(10) == false,
          "Refused allocateSpecific() left the bit untouched");

    // Double allocation: the same block cannot be allocated twice
    // while still in use.
    check(!allocator.allocateSpecific(block),
          "allocateSpecific() refuses an already-allocated block");

    // Freeing releases it; a specific allocation of the same block
    // then succeeds again.
    allocator.free(static_cast<uint64_t>(block));

    check(bitmap.get(static_cast<uint64_t>(block)) == false,
          "free() at/above dataStart_ actually clears the bit");

    check(allocator.allocateSpecific(static_cast<uint64_t>(block)),
          "Block is allocatable again after being freed");

    // Double free: freeing an already-free block must be harmless
    // (idempotent), not corrupt the bitmap or crash.
    allocator.free(static_cast<uint64_t>(block));
    allocator.free(static_cast<uint64_t>(block));

    check(bitmap.get(static_cast<uint64_t>(block)) == false,
          "Double free is idempotent and leaves the block free");

    // Out-of-range block numbers must be rejected, not silently
    // wrapped or read out of bounds.
    check(!allocator.allocateSpecific(totalBlocks),
          "allocateSpecific() rejects a block == totalBlocks");

    check(!allocator.allocateSpecific(UINT64_MAX),
          "allocateSpecific() rejects UINT64_MAX without overflowing");

    // free() on an out-of-range block must not touch memory outside
    // the bitmap's backing storage.
    allocator.free(totalBlocks);
    allocator.free(UINT64_MAX);

    if (failures != 0)
    {
        std::cout << "\n" << failures << " check(s) FAILED\n";
        return 1;
    }

    std::cout << "\nAll BlockAllocator protection checks passed\n";
    return 0;
}
