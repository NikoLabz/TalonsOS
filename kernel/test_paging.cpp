#include "paging.h"
#include "paging_internal.h"
#include "frame_allocator.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

using namespace talons_paging_internal;
using namespace talons_paging;

namespace {

void initFrames()
{
    talons_e820::Entry entries[] = {
        {0x100000, 0x1F00000, talons_e820::TYPE_USABLE, 1}
    };
    talons_e820::Header header = {
        talons_e820::MAGIC, 1, talons_e820::ENTRY_SIZE, 0
    };
    talons_frames::ReservedRange reserved[] = {
        {0x100000, 0x100000}
    };
    assert(talons_frames::init(header, entries, reserved, 1));
}

void testAddressAndSplitHelpers()
{
    assert(recursivePml4Address() == recursiveAddress(510, 510, 510, 510));
    assert(recursivePdptAddress(42) == recursiveAddress(510, 510, 510, 42));
    assert(recursiveDirectoryAddress(12, 34) == recursiveAddress(510, 510, 12, 34));
    assert(recursivePageTableAddress(12, 34, 56) == recursiveAddress(510, 12, 34, 56));
    assert(isCanonical(0x00007FFFFFFFFFFFULL));
    assert(isCanonical(0xFFFF800000000000ULL));
    assert(!isCanonical(0x0000800000000000ULL));
    assert(!isCanonical(0xFFFF7FFFFFFFFFFFULL));
    assert(validPhysicalPage(0));
    assert(validPhysicalPage(0x12345000));
    assert(!validPhysicalPage(0x12345001));
    assert(!validPhysicalPage(0x0010000000000000ULL));

    uint64_t directory[512] = {};
    constexpr uint64_t GIGA_BASE = 0x40000000ULL;
    constexpr uint64_t GIGA_FLAGS = PRESENT | WRITABLE |
                                    USER_ACCESSIBLE | PAGE_SIZE_FLAG;
    assert(split1GiBEntry(GIGA_BASE | GIGA_FLAGS, directory));
    assert(directory[0] == (GIGA_BASE | GIGA_FLAGS));
    assert(directory[511] ==
           (GIGA_BASE + 511 * HUGE_PAGE_SIZE + GIGA_FLAGS));
    assert(!split1GiBEntry(0, directory));
    assert(!split1GiBEntry(GIGA_BASE | GIGA_FLAGS, nullptr));

    uint64_t table[512] = {};
    constexpr uint64_t HUGE_BASE = 0x200000ULL;
    constexpr uint64_t HUGE_FLAGS = PRESENT | WRITABLE |
                                    USER_ACCESSIBLE | PAGE_SIZE_FLAG;
    assert(split2MiBEntry(HUGE_BASE | HUGE_FLAGS, table));
    assert(table[0] == (HUGE_BASE | PRESENT | WRITABLE | USER_ACCESSIBLE));
    assert(table[511] == ((HUGE_BASE + 511 * PAGE_SIZE) |
                          PRESENT | WRITABLE | USER_ACCESSIBLE));
    constexpr uint64_t PAT_HUGE = HUGE_BASE | PRESENT | WRITABLE |
                                  PAGE_SIZE_FLAG | (1ULL << 12);
    assert(split2MiBEntry(PAT_HUGE, table));
    assert(table[0] & PAGE_SIZE_FLAG);
    assert(!(table[0] & (1ULL << 12)));
    assert(!split2MiBEntry(0, table));
    assert(!split2MiBEntry(HUGE_BASE | HUGE_FLAGS, nullptr));
}

void testMappingPermissionsAndReclamation()
{
    constexpr uint64_t VA = 0x0000004000000000ULL;
    constexpr uint64_t PA = 0x00300000ULL;
    PageInfo info{};
    uint64_t physical = 0;

    assert(translate(0x12345, physical) && physical == 0x12345);
    assert(queryPage(0x12345, info));
    assert(info.pageSize == HUGE_PAGE_SIZE);

    const size_t freeBefore = talons_frames::freeFrameCount();
    assert(mapPage(VA, PA, MAP_NO_EXECUTE));
    assert(talons_frames::freeFrameCount() == freeBefore - 2);
    assert(!mapPage(VA, PA + PAGE_SIZE, MAP_WRITABLE));
    assert(queryPage(VA + 0x123, info));
    assert(info.physicalAddress == PA + 0x123);
    assert(info.pageSize == PAGE_SIZE);
    assert((info.flags & MAP_USER) == 0);
    assert((info.flags & MAP_WRITABLE) == 0);
    assert((info.flags & MAP_NO_EXECUTE) != 0);

    const uint64_t before = testing::invalidationCount();
    assert(unmapPage(VA));
    assert(testing::invalidationCount() > before);
    assert(!translate(VA, physical));
    assert(talons_frames::freeFrameCount() == freeBefore);
    assert(!unmapPage(VA));

    assert(!mapPage(0x0000800000000000ULL, PA));
    assert(!mapPage(VA + 1, PA));
    assert(!mapPage(VA, PA + 1));
    assert(!mapPage(VA, PA, 1ULL << 6));
    assert(!mapPage(recursivePml4Address(), PA));
}

void testLargePageUnmapAndFailure()
{
    constexpr uint64_t GIGA_VA = 0x0000008000000000ULL;
    constexpr uint64_t GIGA_PA = 0x40000000ULL;
    constexpr uint64_t target = GIGA_VA + 0x12345000ULL;
    PageInfo info{};
    assert(testing::install1GiBMapping(GIGA_VA, GIGA_PA, MAP_WRITABLE));
    assert(queryPage(target, info));
    assert(info.pageSize == GIGA_PAGE_SIZE);
    assert(info.physicalAddress == GIGA_PA + 0x12345000ULL);

    // Exhaust the PMM: a failed split must leave the 1 GiB mapping intact.
    static uint64_t allocated[8192];
    size_t count = 0;
    while (count < sizeof(allocated) / sizeof(allocated[0]))
    {
        const uint64_t frame = talons_frames::allocate();
        if (!frame) break;
        allocated[count++] = frame;
    }
    assert(count > 0);

    // Leave exactly one frame: mapPage must roll back the first intermediate
    // table when the next page-table allocation fails.
    assert(talons_frames::release(allocated[--count]));
    const size_t oneFrameFree = talons_frames::freeFrameCount();
    constexpr uint64_t NEW_PML4_VA = 0x0000010000000000ULL;
    assert(!mapPage(NEW_PML4_VA, 0x00300000));
    assert(talons_frames::freeFrameCount() == oneFrameFree);
    assert(!queryPage(NEW_PML4_VA, info));
    allocated[count++] = talons_frames::allocate();
    assert(allocated[count - 1] != 0);
    assert(talons_frames::freeFrameCount() == 0);

    assert(!unmapPage(target & ~(PAGE_SIZE - 1)));
    assert(queryPage(target, info) && info.pageSize == GIGA_PAGE_SIZE);
    for (size_t i = 0; i < count; ++i)
        assert(talons_frames::release(allocated[i]));

    assert(unmapPage(target & ~(PAGE_SIZE - 1)));
    uint64_t physical = 0;
    assert(!translate(target, physical));
    assert(queryPage(target + PAGE_SIZE, info));
    assert(info.pageSize == PAGE_SIZE);
    assert(info.physicalAddress == GIGA_PA + 0x12345000ULL + PAGE_SIZE);
    assert(queryPage(GIGA_VA + 0x1000, info));
    assert(info.physicalAddress == GIGA_PA + 0x1000);
}

void testTemporaryFramesAndZeroing()
{
    const size_t before = talons_frames::freeFrameCount();
    const uint64_t frame = talons_frames::allocate();
    assert(frame);
    uint8_t* bytes = static_cast<uint8_t*>(mapTemporaryFrame(frame));
    assert(bytes);
    bytes[0] = 0x5A;
    assert(mapTemporaryFrame(frame) == nullptr);
    assert(unmapTemporaryFrame());
    assert(!unmapTemporaryFrame());
    assert(talons_frames::release(frame));
    assert(talons_frames::freeFrameCount() == before);

    const uint64_t zeroed = allocateZeroedFrame();
    assert(zeroed);
    bytes = static_cast<uint8_t*>(mapTemporaryFrame(zeroed));
    assert(bytes);
    for (size_t i = 0; i < PAGE_SIZE; ++i) assert(bytes[i] == 0);
    assert(unmapTemporaryFrame());
    assert(talons_frames::release(zeroed));

    constexpr uint64_t VA = 0x0000006000000000ULL;
    uint64_t mappedFrame = 0;
    assert(mapNewPage(VA, mappedFrame, MAP_WRITABLE));
    PageInfo mappedInfo{};
    assert(mappedFrame && queryPage(VA, mappedInfo));
    assert(unmapPage(VA));
    assert(talons_frames::release(mappedFrame));

    // Existing mappings are never replaced; the allocated frame is returned.
    assert(mapPage(VA, 0x00400000));
    assert(!mapNewPage(VA, mappedFrame));
    assert(mappedFrame == 0);
    assert(unmapPage(VA));
}

void testIndependentUserAddressSpaces()
{
    AddressSpace first{};
    AddressSpace second{};
    const uint64_t kernelRoot = activeAddressSpace();
    const size_t freeBefore = talons_frames::freeFrameCount();
    assert(createAddressSpace(first));
    assert(createAddressSpace(second));
    assert(first.valid && second.valid && first.rootPhysical != second.rootPhysical);
    assert(activeAddressSpace() == kernelRoot);

    constexpr uint64_t USER_VA = 0x0000004000000000ULL;
    uint64_t firstFrame = 0;
    uint64_t secondFrame = 0;
    assert(mapUserPage(first, USER_VA, firstFrame,
                       MAP_USER | MAP_WRITABLE | MAP_NO_EXECUTE));
    assert(mapUserPage(second, USER_VA, secondFrame,
                       MAP_USER | MAP_NO_EXECUTE));
    assert(firstFrame && secondFrame && firstFrame != secondFrame);
    assert(activeAddressSpace() == kernelRoot);

    PageInfo info{};
    assert(queryAddressSpace(first, USER_VA, info));
    assert((info.flags & MAP_USER) && (info.flags & MAP_WRITABLE));
    assert(info.flags & MAP_NO_EXECUTE);
    assert(queryAddressSpace(second, USER_VA, info));
    assert((info.flags & MAP_USER) && !(info.flags & MAP_WRITABLE));
    assert(!queryAddressSpace(first, USER_VA + PAGE_SIZE, info));

    // The shared kernel identity mapping survives, with supervisor rights.
    assert(queryAddressSpace(first, 0x12345, info));
    assert(!(info.flags & MAP_USER));
    assert(!mapUserPage(first, USER_END, firstFrame));
    assert(!mapUserPage(first, USER_VA + 1, firstFrame));
    assert(!mapUserPage(first, USER_VA, firstFrame));
    assert(!mapPage(USER_END, 0x00300000, MAP_USER));

    assert(!activateAddressSpace(AddressSpace{}));
    AddressSpace activeCopy = first;
    assert(activateAddressSpace(activeCopy));
    assert(activeAddressSpace() == first.rootPhysical);
    assert(queryPage(USER_VA, info) && (info.flags & MAP_USER));
    assert(!mapPage(USER_VA + PAGE_SIZE, 0x00300000, MAP_WRITABLE));
    assert(!unmapPage(USER_VA));
    assert(activateAddressSpace(AddressSpace{kernelRoot, true}));
    assert(activeAddressSpace() == kernelRoot);

    const size_t beforeDestroy = talons_frames::freeFrameCount();
    assert(destroyAddressSpace(first));
    assert(!first.valid && !queryAddressSpace(first, USER_VA, info));
    assert(talons_frames::freeFrameCount() == beforeDestroy + 5);
    const size_t beforeUnmap = talons_frames::freeFrameCount();
    assert(unmapUserPage(second, USER_VA));
    assert(talons_frames::freeFrameCount() == beforeUnmap + 3);
    assert(!queryAddressSpace(second, USER_VA, info));
    assert(activateAddressSpace(second));
    assert(destroyAddressSpace(second));
    assert(activeAddressSpace() == kernelRoot);
    assert(!second.valid);
    assert(talons_frames::freeFrameCount() == freeBefore);
    assert(queryPage(0x12345, info));

    static uint64_t held[8192];
    size_t heldCount = 0;
    while (heldCount < sizeof(held) / sizeof(held[0]))
    {
        const uint64_t frame = talons_frames::allocate();
        if (!frame) break;
        held[heldCount++] = frame;
    }
    assert(heldCount > 0);
    assert(talons_frames::release(held[--heldCount]));
    assert(!createAddressSpace(first));
    assert(!first.valid && talons_frames::freeFrameCount() == 1);
    while (heldCount) assert(talons_frames::release(held[--heldCount]));

    assert(createAddressSpace(first));
    heldCount = 0;
    while (heldCount < sizeof(held) / sizeof(held[0]))
    {
        const uint64_t frame = talons_frames::allocate();
        if (!frame) break;
        held[heldCount++] = frame;
    }
    assert(heldCount > 3);
    for (size_t i = 0; i < 3; ++i)
        assert(talons_frames::release(held[--heldCount]));
    const size_t constrainedFree = talons_frames::freeFrameCount();
    assert(constrainedFree == 3);
    uint64_t failedFrame = 0;
    assert(!mapUserPage(first, USER_VA, failedFrame,
                        MAP_USER | MAP_WRITABLE | MAP_NO_EXECUTE));
    assert(failedFrame == 0 && talons_frames::freeFrameCount() == constrainedFree);
    assert(destroyAddressSpace(first));
    while (heldCount) assert(talons_frames::release(held[--heldCount]));
}

} // namespace

int main()
{
    testAddressAndSplitHelpers();
    initFrames();
    assert(init());
    assert(isInitialised());
    testMappingPermissionsAndReclamation();
    testLargePageUnmapAndFailure();
    testTemporaryFramesAndZeroing();
    testIndependentUserAddressSpaces();
    std::puts("Paging and virtual memory tests passed.");
    return 0;
}
