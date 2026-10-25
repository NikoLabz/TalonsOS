#include <cassert>
#include <cstdint>
#include "e820.h"

int main()
{
    using namespace talons_e820;

    static_assert(sizeof(Header) == 16);
    static_assert(sizeof(Entry) == 24);

    Header header{MAGIC, 3, ENTRY_SIZE, 0};
    assert(validHeader(header));

    header.magic = 0;
    assert(!validHeader(header));

    header = {MAGIC, 0, ENTRY_SIZE, 0};
    assert(!validHeader(header));

    header = {MAGIC, 65, ENTRY_SIZE, 0};
    assert(!validHeader(header));

    header = {MAGIC, 1, 20, 0};
    assert(!validHeader(header));

    Entry usable{0x200000, 0x100000, TYPE_USABLE, 1};
    assert(isUsable(usable));

    Entry reserved{0x200000, 0x100000, 2, 1};
    assert(!isUsable(reserved));

    Entry disabled{0x200000, 0x100000, TYPE_USABLE, 0};
    assert(!isUsable(disabled));

    Entry empty{0x200000, 0, TYPE_USABLE, 1};
    assert(!isUsable(empty));

    Entry overflow{UINT64_MAX - 7, 16, TYPE_USABLE, 1};
    assert(!isUsable(overflow));

    Entry lastByte{UINT64_MAX - 1, 1, TYPE_USABLE, 1};
    assert(isUsable(lastByte));

        // Test complete E820 map validation.
    Entry entries[] = {
        {0x100000, 0x100000, TYPE_USABLE, 1},
        {0x200000, 0x100000, 2, 1},
        {0x300000, 0, TYPE_USABLE, 1}
    };

    Header validMapHeader{MAGIC, 3, ENTRY_SIZE, 0};
    assert(validMap(validMapHeader, entries));

    // A null entry array must be rejected.
    assert(!validMap(validMapHeader, nullptr));

    // Invalid headers must be rejected.
    Header invalidMapHeader{0, 3, ENTRY_SIZE, 0};
    assert(!validMap(invalidMapHeader, entries));

    // An overflowing address range must be rejected.
    Entry badRange[] = {
        {UINT64_MAX - 7, 16, TYPE_USABLE, 1}
    };
    Header badRangeHeader{MAGIC, 1, ENTRY_SIZE, 0};
    assert(!validMap(badRangeHeader, badRange));

    // A range must be fully contained in one usable entry.
    Entry containingRegion{0x200000, 0x400000, TYPE_USABLE, 1};

    assert(usableContainsRange(containingRegion, 0x200000, 0x400000));
    assert(usableContainsRange(containingRegion, 0x300000, 0x100000));
    assert(usableContainsRange(containingRegion, 0x5FFFFF, 1));

    // Ranges extending beyond either boundary must fail.
    assert(!usableContainsRange(containingRegion, 0x1FFFFF, 2));
    assert(!usableContainsRange(containingRegion, 0x5FFFFF, 2));
    assert(!usableContainsRange(containingRegion, 0x700000, 0x100000));

    // Invalid or overflowing requests must fail.
    assert(!usableContainsRange(containingRegion, 0x300000, 0));
    assert(!usableContainsRange(containingRegion, UINT64_MAX - 7, 16));
    assert(!usableContainsRange(reserved, 0x200000, 0x100000));
    assert(!usableContainsRange(disabled, 0x200000, 0x100000));

    // Intersect a usable region with a requested range.
    Entry region{0x200000, 0x400000, TYPE_USABLE, 1};

    uint64_t resultBase = 0;
    uint64_t resultLength = 0;

    assert(usableIntersection(
        region, 0x300000, 0x100000,
        resultBase, resultLength));
    assert(resultBase == 0x300000);
    assert(resultLength == 0x100000);

    // Partial overlap at the beginning.
    assert(usableIntersection(
        region, 0x100000, 0x200000,
        resultBase, resultLength));
    assert(resultBase == 0x200000);
    assert(resultLength == 0x100000);

    // No overlap.
    assert(!usableIntersection(
        region, 0x700000, 0x100000,
        resultBase, resultLength));
    assert(resultLength == 0);

    // Reserved regions must never be considered usable.
    assert(!usableIntersection(
        reserved, 0x200000, 0x100000,
        resultBase, resultLength));

    // Overflowing requested ranges must be rejected.
    assert(!usableIntersection(
        region, UINT64_MAX - 7, 16,
        resultBase, resultLength));


    // A disabled non-usable entry must still block overlapping memory.
    Entry disabledReservedMap[] = {
        {0x200000, 0x400000, TYPE_USABLE, 1},
        {0x300000, 0x100000, 2, 0}
    };
    Header disabledReservedHeader{MAGIC, 2, ENTRY_SIZE, 0};
    assert(!hasNoReservedOverlap(
        disabledReservedHeader, disabledReservedMap,
        0x300000, 0x100000));

    // Combined check: contained in usable RAM and no reserved overlap.
    Entry combinedMap[] = {
        {0x200000, 0x400000, TYPE_USABLE, 1},
        {0x300000, 0x100000, 2, 1}
    };
    Header combinedHeader{MAGIC, 2, ENTRY_SIZE, 0};

    assert(mapRangeIsUsable(
        combinedHeader, combinedMap, combinedMap[0],
        0x200000, 0x80000));
    assert(!mapRangeIsUsable(
        combinedHeader, combinedMap, combinedMap[0],
        0x280000, 0x100000));
    assert(!mapRangeIsUsable(
        combinedHeader, combinedMap, combinedMap[0],
        0x5FFFFF, 2));
    assert(!mapRangeIsUsable(
        combinedHeader, nullptr, combinedMap[0],
        0x200000, 0x1000));

    // Reserved regions must block overlapping candidate ranges.
    Entry overlapMap[] = {
        {0x200000, 0x400000, TYPE_USABLE, 1},
        {0x300000, 0x100000, 2, 1}
    };
    Header overlapHeader{MAGIC, 2, ENTRY_SIZE, 0};

    assert(!hasNoReservedOverlap(
        overlapHeader, overlapMap, 0x280000, 0x100000));
    assert(hasNoReservedOverlap(
        overlapHeader, overlapMap, 0x400000, 0x100000));
    assert(!hasNoReservedOverlap(
        overlapHeader, overlapMap, UINT64_MAX - 7, 16));

    return 0;
}
