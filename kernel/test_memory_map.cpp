#include "memory_map.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace talons_e820;
using namespace talons_memmap;

namespace
{
    constexpr uint64_t KIB = 1024;
    constexpr uint64_t MIB = 1024 * KIB;
    constexpr uint64_t GIB = 1024 * MIB;

    bool equal(const char* a, const char* b)
    {
        return std::strcmp(a, b) == 0;
    }
}

int main()
{
    // 1. Type names.
    assert(equal(typeName(1), "Usable"));
    assert(equal(typeName(2), "Reserved"));
    assert(equal(typeName(3), "ACPI reclaimable"));
    assert(equal(typeName(4), "ACPI NVS"));
    assert(equal(typeName(5), "Bad memory"));
    assert(equal(typeName(0), "Unknown"));
    assert(equal(typeName(99), "Unknown"));

    // 2. Typical QEMU-style map (128 MiB guest).
    {
        Entry map[] = {
            {0x0,         0x9FC00,      1, 1},
            {0x9FC00,     0x400,        2, 1},
            {0xF0000,     0x10000,      2, 1},
            {0x100000,    0x7EE0000,    1, 1},
            {0x7FE0000,   0x20000,      2, 1},
            {0xFFFC0000,  0x40000,      2, 1},
        };
        Header header{MAGIC, 6, ENTRY_SIZE, 0};

        Summary s;
        assert(summarise(header, map, s));
        assert(s.entryCount == 6);
        assert(s.emptyEntries == 0);
        assert(s.usableEntries == 2);
        assert(s.reservedEntries == 4);
        assert(s.disabledEntries == 0);
        assert(s.usableBytes == 0x9FC00 + 0x7EE0000);
        assert(s.reservedBytes == 0x400 + 0x10000 + 0x20000 + 0x40000);
        assert(s.highestUsableEnd == 0x7FE0000);
        assert(s.highestEnd == 0x100000000ULL);
        assert(!s.hasOverlap);
    }

    // 3. Entry order does not matter.
    {
        Entry map[] = {
            {0x100000, 0x100000, 1, 1},
            {0x0,      0x80000,  1, 1},
        };
        Header header{MAGIC, 2, ENTRY_SIZE, 0};

        Summary s;
        assert(summarise(header, map, s));
        assert(s.usableBytes == 0x180000);
        assert(s.highestUsableEnd == 0x200000);
        assert(!s.hasOverlap);
    }

    // 4. Empty and disabled entries are counted separately and do not
    //    contribute to usable/reserved totals or the highest address.
    {
        Entry map[] = {
            {0x0,        0x80000,  1, 1},
            {0x80000,    0,        1, 1},   // empty
            {0x100000,   0x100000, 1, 0},   // usable but disabled
            {0x200000,   0x100000, 2, 0},   // reserved but disabled
            {0x300000,   0x1000,   2, 1},
        };
        Header header{MAGIC, 5, ENTRY_SIZE, 0};

        Summary s;
        assert(summarise(header, map, s));
        assert(s.emptyEntries == 1);
        assert(s.disabledEntries == 2);
        assert(s.disabledBytes == 0x200000);
        assert(s.usableEntries == 1);
        assert(s.usableBytes == 0x80000);
        assert(s.reservedEntries == 1);
        assert(s.reservedBytes == 0x1000);
        assert(s.highestUsableEnd == 0x80000);
        assert(s.highestEnd == 0x301000);
        assert(!s.hasOverlap);
    }

    // 5. Overlap detection: overlapping ranges are flagged, merely
    //    adjacent ranges are not, and an empty entry never overlaps.
    {
        Entry adjacent[] = {
            {0x0,     0x1000, 1, 1},
            {0x1000,  0x1000, 2, 1},
        };
        Header header{MAGIC, 2, ENTRY_SIZE, 0};
        Summary s;
        assert(summarise(header, adjacent, s));
        assert(!s.hasOverlap);

        Entry overlapping[] = {
            {0x0,     0x2000, 1, 1},
            {0x1000,  0x1000, 2, 1},
        };
        assert(summarise(header, overlapping, s));
        assert(s.hasOverlap);

        Entry contained[] = {
            {0x1000,  0x1000, 2, 1},
            {0x0,     0x4000, 1, 1},
        };
        assert(summarise(header, contained, s));
        assert(s.hasOverlap);

        Entry withEmpty[] = {
            {0x0,     0x2000, 1, 1},
            {0x1000,  0,      2, 1},
        };
        assert(summarise(header, withEmpty, s));
        assert(!s.hasOverlap);

        // A disabled entry that overlaps is still reported, matching
        // frame_allocator which treats any overlap as ambiguous.
        Entry disabledOverlap[] = {
            {0x0,     0x2000, 1, 1},
            {0x1000,  0x1000, 2, 0},
        };
        assert(summarise(header, disabledOverlap, s));
        assert(s.hasOverlap);
    }

    // 6. Totals saturate instead of wrapping.
    {
        Entry map[] = {
            {0x0, 0x8000000000000000ULL, 1, 1},
            {0x0, 0x8000000000000000ULL, 1, 1},
        };
        Header header{MAGIC, 2, ENTRY_SIZE, 0};

        Summary s;
        assert(summarise(header, map, s));
        assert(s.usableBytes == UINT64_MAX);
        assert(s.hasOverlap);

        assert(saturatingAdd(1, 2) == 3);
        assert(saturatingAdd(UINT64_MAX, 1) == UINT64_MAX);
        assert(saturatingAdd(UINT64_MAX, UINT64_MAX) == UINT64_MAX);
    }

    // 7. Invalid input is rejected and the summary is zeroed.
    {
        Entry map[] = {{0x0, 0x1000, 1, 1}};

        Summary s;
        s.usableBytes = 123;
        s.hasOverlap = true;

        Header badMagic{0, 1, ENTRY_SIZE, 0};
        assert(!summarise(badMagic, map, s));
        assert(s.usableBytes == 0 && !s.hasOverlap && s.entryCount == 0);

        Header zeroCount{MAGIC, 0, ENTRY_SIZE, 0};
        assert(!summarise(zeroCount, map, s));

        Header tooMany{MAGIC, MAX_ENTRIES + 1, ENTRY_SIZE, 0};
        assert(!summarise(tooMany, map, s));

        Header good{MAGIC, 1, ENTRY_SIZE, 0};
        assert(!summarise(good, nullptr, s));

        Entry overflowing[] = {{UINT64_MAX - 1, 16, 1, 1}};
        assert(!summarise(good, overflowing, s));
    }

    // 8. A maximum-size map (64 entries) is handled.
    {
        Entry map[MAX_ENTRIES];
        for (uint32_t i = 0; i < MAX_ENTRIES; ++i)
            map[i] = Entry{i * GIB, MIB, 1, 1};

        Header header{MAGIC, MAX_ENTRIES, ENTRY_SIZE, 0};

        Summary s;
        assert(summarise(header, map, s));
        assert(s.entryCount == MAX_ENTRIES);
        assert(s.usableEntries == MAX_ENTRIES);
        assert(s.usableBytes == MAX_ENTRIES * MIB);
        assert(s.highestUsableEnd == (MAX_ENTRIES - 1) * GIB + MIB);
        assert(!s.hasOverlap);
    }

    std::puts("Memory map summary tests passed.");
    return 0;
}
