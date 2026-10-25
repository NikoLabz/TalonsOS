#include "frame_allocator.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

using namespace talons_frames;
using talons_e820::Entry;
using talons_e820::Header;

int main()
{
    // Synthetic E820 map: 1 MiB to 4 MiB usable.
    Entry entries[] = {
        {0x100000, 0x300000, talons_e820::TYPE_USABLE, 1},
        // A reserved overlap must override the usable entry.
        {0x200000, 0x1000, 2, 1}
    };

    Header header = {
        talons_e820::MAGIC,
        2,
        talons_e820::ENTRY_SIZE,
        0
    };

    ReservedRange reserved[] = {
        // Simulate the existing 1 MiB kernel heap.
        {0x100000, 0x100000},
        // Simulate page tables.
        {0x90000, 0x3000}
    };

    assert(!isInitialised());
    assert(allocate() == 0);

    assert(init(header, entries, reserved, 2));
    assert(isInitialised());

    // The heap range is reserved, so allocation must start later.
    uint64_t first = allocate();
    assert(first != 0);
    assert(first % FRAME_SIZE == 0);
    assert(first >= 0x200000);
    assert(first != 0x200000); // Reserved E820 overlap.

    uint64_t second = allocate();
    assert(second != 0 && second != first);
    assert(freeFrameCount() > 0);

    // Valid free succeeds; duplicate and unaligned frees fail.
    assert(release(first));
    assert(!release(first));
    assert(!release(second + 1));
    assert(!release(MAX_PHYSICAL_ADDRESS));

    // Freed frame can be allocated again.
    uint64_t reused = allocate();
    assert(reused == first);

    // Invalid E820 map must not initialise the allocator.
    Header invalid = header;
    invalid.magic = 0;
    assert(!init(invalid, entries, nullptr, 0));
    assert(!isInitialised());
    assert(allocate() == 0);

    // Restore the valid map and test a reserved-range overflow.
    ReservedRange badRange[] = {
        {UINT64_MAX - 10, 100}
    };
    assert(!init(header, entries, badRange, 1));
    assert(!isInitialised());

    // Reinitialise and exhaust all available frames.
    assert(init(header, entries, reserved, 2));

    size_t allocatedCount = 0;
    while (allocate() != 0)
        ++allocatedCount;

    assert(allocatedCount > 0);
    assert(freeFrameCount() == 0);
    assert(allocate() == 0);


    // Overlapping usable E820 entries are ambiguous and must be excluded.
    Entry overlapping[] = {
        {0x100000, 0x4000, talons_e820::TYPE_USABLE, 1},
        {0x102000, 0x2000, talons_e820::TYPE_USABLE, 1}
    };
    Header overlapHeader = {
        talons_e820::MAGIC, 2, talons_e820::ENTRY_SIZE, 0
    };
    assert(init(overlapHeader, overlapping, nullptr, 0));
    assert(allocate() == 0x100000);
    assert(allocate() == 0x101000);
    assert(allocate() == 0);
    assert(!release(0));

    // Reserved ranges above the managed limit must be safely ignored.
    Entry smallMap[] = {
        {0x100000, 0x4000, talons_e820::TYPE_USABLE, 1}
    };
    Header smallHeader = {
        talons_e820::MAGIC, 1, talons_e820::ENTRY_SIZE, 0
    };
    ReservedRange aboveLimit[] = {
        {MAX_PHYSICAL_ADDRESS + 0x1000, 0x2000}
    };
    assert(init(smallHeader, smallMap, aboveLimit, 1));
    assert(allocate() == 0x100000);

    std::puts("Physical frame allocator tests passed.");
    return 0;
}
