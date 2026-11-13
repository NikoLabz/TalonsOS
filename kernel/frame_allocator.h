#pragma once

#include "e820.h"

#include <stddef.h>
#include <stdint.h>

namespace talons_frames
{
    constexpr uint64_t FRAME_SIZE = 4096;
    constexpr uint64_t MAX_PHYSICAL_ADDRESS = 1024ULL * 1024 * 1024;
    constexpr size_t MAX_FRAMES =
        MAX_PHYSICAL_ADDRESS / FRAME_SIZE;
    constexpr size_t BITMAP_SIZE = MAX_FRAMES / 8;

    struct ReservedRange
    {
        uint64_t base;
        uint64_t length;
    };

    // Initialise from a validated E820 map and explicit OS-reserved ranges.
    // Returns false for invalid input or invalid ranges.
    bool init(
        const talons_e820::Header& header,
        const talons_e820::Entry* entries,
        const ReservedRange* reserved,
        size_t reservedCount);

    // Returns a physical address aligned to FRAME_SIZE, or 0 on failure.
    uint64_t allocate();

    // Only frees a frame previously allocated by this allocator.
    bool release(uint64_t physicalAddress);

    size_t freeFrameCount();
    bool isInitialised();

    // ---- Phase 5 M2 additions (existing functions above unchanged) ----

    // Frame 0 is always reserved by init(), regardless of the E820 map or
    // the caller's reserved ranges, so that allocate()'s failure value (0)
    // can never be a valid frame address.

    // Counters are in frames unless noted. The allocator tracks every
    // frame individually: managedFrames == freeFrames + allocatedFrames
    // always holds.
    struct Stats
    {
        uint64_t managedFrames;        // frames made available by init()
        uint64_t freeFrames;
        uint64_t allocatedFrames;      // currently allocated
        uint64_t largestFreeRun;       // longest run of adjacent free frames
        uint64_t framesAllocatedTotal; // lifetime, since init()
        uint64_t framesReleasedTotal;  // lifetime, since init()
        uint64_t failedAllocations;    // allocation CALLS that returned 0
        uint64_t rejectedReleases;     // release CALLS refused (double free,
                                       // unaligned, never allocated, ...)
        uint64_t untrackedUsableBytes; // usable E820 bytes at or above
                                       // MAX_PHYSICAL_ADDRESS (not managed;
                                       // may double count overlapping entries)
    };

    // Fills 'out'. Returns false (and zeroes 'out') if not initialised.
    bool getStats(Stats& out);

    // Allocates 'count' physically adjacent frames; returns the address of
    // the first one, or 0 on failure (count == 0, count too large, or no
    // free run). Always returns the lowest-addressed fit.
    // allocateContiguous(1) behaves exactly like allocate().
    uint64_t allocateContiguous(size_t count);

    // Releases 'count' frames starting at 'physicalAddress'. All-or-nothing:
    // if any frame in the range is not currently allocated (or the range is
    // invalid) nothing is released and false is returned. The range need
    // not match an earlier allocateContiguous() call.
    bool releaseContiguous(uint64_t physicalAddress, size_t count);

    // Frame state queries; false for unaligned, out-of-range or
    // uninitialised input.
    bool isAllocated(uint64_t physicalAddress);
    bool isFree(uint64_t physicalAddress);

    // Internal consistency check (O(MAX_FRAMES)). Returns false if the
    // allocator is not initialised or its bookkeeping is inconsistent:
    // allocated frames not marked unavailable, free count mismatch,
    // frame 0 free, a free frame below the search hint, or
    // free + allocated != managed.
    bool verify();
}
