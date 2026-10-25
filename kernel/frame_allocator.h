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
}
