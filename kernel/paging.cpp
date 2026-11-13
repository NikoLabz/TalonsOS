#include "paging.h"
#include "paging_internal.h"
#include "frame_allocator.h"

namespace talons_paging {
namespace {

using namespace talons_paging_internal;

constexpr uint64_t PML4_ADDRESS = 0x90000;
constexpr uint64_t PDPT_ADDRESS = 0x91000;
constexpr uint64_t PAGE_DIRECTORY_ADDRESS = 0x92000;
constexpr uint16_t RECURSIVE_SLOT = 510;
constexpr uint64_t TEMPORARY_WINDOW = 0xFFFF800000000000ULL;
constexpr uint64_t ONE_GIB = 1ULL << 30;
constexpr uint64_t ONE_GIB_ADDRESS_MASK = 0x000FFFFFC0000000ULL;
bool initialised = false;
bool temporaryActive = false;
bool nxEnabled = false;
uint64_t activeRootPhysical = PML4_ADDRESS;
bool userMappingInProgress = false;
#ifdef TALONS_PAGING_HOST_TEST
uint64_t invalidations = 0;

struct SimTable
{
    uint64_t physical;
    bool used;
    uint64_t entries[512];
};

constexpr size_t SIM_TABLE_COUNT = 1024;
SimTable simTables[SIM_TABLE_COUNT] = {};
uint64_t simPml4[512] = {};
uint64_t simPdpt[512] = {};
uint64_t simDirectory[512] = {};

struct SimFrame
{
    uint64_t physical;
    bool used;
    uint8_t bytes[PAGE_SIZE];
};

constexpr size_t SIM_FRAME_COUNT = 1024;
SimFrame simFrames[SIM_FRAME_COUNT] = {};

uint64_t* simTable(uint64_t physical)
{
    if (physical == PML4_ADDRESS) return simPml4;
    if (physical == PDPT_ADDRESS) return simPdpt;
    if (physical == PAGE_DIRECTORY_ADDRESS) return simDirectory;

    for (size_t i = 0; i < SIM_TABLE_COUNT; ++i)
        if (simTables[i].used && simTables[i].physical == physical)
            return simTables[i].entries;
    return nullptr;
}

uint64_t* registerSimTable(uint64_t physical)
{
    uint64_t* existing = simTable(physical);
    if (existing) return existing;

    for (size_t i = 0; i < SIM_TABLE_COUNT; ++i)
    {
        if (simTables[i].used) continue;
        simTables[i].physical = physical;
        simTables[i].used = true;
        for (size_t j = 0; j < 512; ++j)
            simTables[i].entries[j] = 0;
        return simTables[i].entries;
    }
    return nullptr;
}

void unregisterSimTable(uint64_t physical)
{
    for (size_t i = 0; i < SIM_TABLE_COUNT; ++i)
    {
        if (simTables[i].used && simTables[i].physical == physical)
        {
            simTables[i].used = false;
            return;
        }
    }
}

uint8_t* simFrame(uint64_t physical)
{
    for (size_t i = 0; i < SIM_FRAME_COUNT; ++i)
        if (simFrames[i].used && simFrames[i].physical == physical)
            return simFrames[i].bytes;

    for (size_t i = 0; i < SIM_FRAME_COUNT; ++i)
    {
        if (simFrames[i].used) continue;
        simFrames[i].used = true;
        simFrames[i].physical = physical;
        for (size_t j = 0; j < PAGE_SIZE; ++j)
            simFrames[i].bytes[j] = 0xA5;
        return simFrames[i].bytes;
    }
    return nullptr;
}
#endif

uint64_t recursiveAddress(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
    return talons_paging_internal::recursiveAddress(a, b, c, d);
}

uint64_t* pml4Table()
{
#ifdef TALONS_PAGING_HOST_TEST
    return simTable(activeRootPhysical);
#else
    return reinterpret_cast<uint64_t*>(recursiveAddress(
        RECURSIVE_SLOT, RECURSIVE_SLOT, RECURSIVE_SLOT, RECURSIVE_SLOT));
#endif
}

uint64_t* pdptTable(uint16_t a)
{
#ifdef TALONS_PAGING_HOST_TEST
    const uint64_t* root = pml4Table();
    const uint64_t entry = root ? root[a] : 0;
    return (entry & PRESENT) ? simTable(entry & ADDRESS_4K_MASK) : nullptr;
#else
    return reinterpret_cast<uint64_t*>(recursivePdptAddress(a));
#endif
}

uint64_t* directoryTable(uint16_t a, uint16_t b)
{
#ifdef TALONS_PAGING_HOST_TEST
    uint64_t* pdpt = pdptTable(a);
    if (!pdpt || !(pdpt[b] & PRESENT) || (pdpt[b] & PAGE_SIZE_FLAG))
        return nullptr;
    return simTable(pdpt[b] & ADDRESS_4K_MASK);
#else
    return reinterpret_cast<uint64_t*>(recursiveDirectoryAddress(a, b));
#endif
}

uint64_t* pageTable(uint16_t a, uint16_t b, uint16_t c)
{
#ifdef TALONS_PAGING_HOST_TEST
    uint64_t* directory = directoryTable(a, b);
    if (!directory || !(directory[c] & PRESENT) ||
        (directory[c] & PAGE_SIZE_FLAG))
        return nullptr;
    return simTable(directory[c] & ADDRESS_4K_MASK);
#else
    return reinterpret_cast<uint64_t*>(recursiveAddress(
        RECURSIVE_SLOT, a, b, c));
#endif
}

void invalidatePage(uint64_t address)
{
#ifdef TALONS_PAGING_HOST_TEST
    (void)address;
    ++invalidations;
#else
    asm volatile("invlpg (%0)" : : "r"(address) : "memory");
#endif
}

uint64_t recursiveTableAddress(unsigned int level,
                               uint16_t a, uint16_t b, uint16_t c)
{
    if (level == 3) return recursivePdptAddress(a);
    if (level == 2) return recursiveDirectoryAddress(a, b);
    return recursiveAddress(RECURSIVE_SLOT, a, b, c);
}

struct CreatedTable
{
    uint64_t* parent;
    uint64_t frame;
    uint64_t recursiveVirtual;
};

bool createTable(uint64_t& parent, uint64_t recursiveVirtual,
                 uint64_t& frame)
{
    frame = talons_frames::allocate();
    if (!frame) return false;

#ifdef TALONS_PAGING_HOST_TEST
    if (!registerSimTable(frame))
    {
        talons_frames::release(frame);
        return false;
    }
#endif

    // Attach first so the recursive alias becomes valid, then invalidate
    // that alias before clearing the newly allocated table through it.
    parent = frame | PRESENT | WRITABLE;
    invalidatePage(recursiveVirtual);
#ifdef TALONS_PAGING_HOST_TEST
    uint64_t* table = simTable(frame);
#else
    uint64_t* table = reinterpret_cast<uint64_t*>(recursiveVirtual);
#endif
    if (!table)
    {
        parent = 0;
#ifdef TALONS_PAGING_HOST_TEST
        unregisterSimTable(frame);
#endif
        talons_frames::release(frame);
        frame = 0;
        return false;
    }
    for (size_t i = 0; i < 512; ++i) table[i] = 0;
    return true;
}

void rollbackTables(CreatedTable* created, size_t count)
{
    while (count)
    {
        const CreatedTable item = created[--count];
        *item.parent = 0;
        invalidatePage(item.recursiveVirtual);
#ifdef TALONS_PAGING_HOST_TEST
        unregisterSimTable(item.frame);
#endif
        talons_frames::release(item.frame);
    }
}

bool tableEmpty(const uint64_t* table)
{
    for (size_t i = 0; i < 512; ++i)
        if (table[i] & PRESENT) return false;
    return true;
}

void reclaimTable(uint64_t& parent, uint64_t frame,
                  uint64_t recursiveVirtual)
{
    // Boot tables and any externally supplied tables are reserved rather
    // than allocator-owned. Only detach and release PMM-owned tables.
    if (!talons_frames::isAllocated(frame)) return;
    parent = 0;
    invalidatePage(recursiveVirtual);
#ifdef TALONS_PAGING_HOST_TEST
    unregisterSimTable(frame);
#endif
    talons_frames::release(frame);
}

bool splitOneGiB(uint64_t& entry, uint16_t a, uint16_t b,
                 uint64_t& tableFrame)
{
    const uint64_t old = entry;
    if (!(old & PRESENT) || !(old & PAGE_SIZE_FLAG)) return false;
    tableFrame = talons_frames::allocate();
    if (!tableFrame) return false;
#ifdef TALONS_PAGING_HOST_TEST
    if (!registerSimTable(tableFrame))
    {
        talons_frames::release(tableFrame);
        tableFrame = 0;
        return false;
    }
#endif
    entry = tableFrame | PRESENT | WRITABLE |
            (old & (USER_ACCESSIBLE | NO_EXECUTE));
    const uint64_t recursive = recursiveTableAddress(2, a, b, 0);
    invalidatePage(recursive);
    uint64_t* directory = directoryTable(a, b);
#ifdef TALONS_PAGING_HOST_TEST
    directory = simTable(tableFrame);
#else
    directory = reinterpret_cast<uint64_t*>(recursive);
#endif
    if (!directory)
    {
        entry = old;
#ifdef TALONS_PAGING_HOST_TEST
        unregisterSimTable(tableFrame);
#endif
        talons_frames::release(tableFrame);
        tableFrame = 0;
        return false;
    }
    split1GiBEntry(old, directory);
    return true;
}

bool splitTwoMiB(uint64_t& entry, uint16_t a, uint16_t b, uint16_t c,
                 uint64_t& tableFrame)
{
    const uint64_t old = entry;
    if (!(old & PRESENT) || !(old & PAGE_SIZE_FLAG)) return false;
    tableFrame = talons_frames::allocate();
    if (!tableFrame) return false;
#ifdef TALONS_PAGING_HOST_TEST
    if (!registerSimTable(tableFrame))
    {
        talons_frames::release(tableFrame);
        tableFrame = 0;
        return false;
    }
#endif
    entry = tableFrame | PRESENT | WRITABLE |
            (old & (USER_ACCESSIBLE | NO_EXECUTE));
    const uint64_t recursive = recursiveTableAddress(1, a, b, c);
    invalidatePage(recursive);
#ifdef TALONS_PAGING_HOST_TEST
    uint64_t* table = simTable(tableFrame);
#else
    uint64_t* table = reinterpret_cast<uint64_t*>(recursive);
#endif
    if (!table)
    {
        entry = old;
#ifdef TALONS_PAGING_HOST_TEST
        unregisterSimTable(tableFrame);
#endif
        talons_frames::release(tableFrame);
        tableFrame = 0;
        return false;
    }
    split2MiBEntry(old, table);
    return true;
}

} // namespace

bool init()
{
#ifdef TALONS_PAGING_HOST_TEST
    for (size_t i = 0; i < SIM_TABLE_COUNT; ++i) simTables[i].used = false;
    for (size_t i = 0; i < SIM_FRAME_COUNT; ++i) simFrames[i].used = false;
    for (size_t i = 0; i < 512; ++i)
    {
        simPml4[i] = 0;
        simPdpt[i] = 0;
        simDirectory[i] = 0;
    }
    simPml4[0] = PDPT_ADDRESS | PRESENT | WRITABLE;
    simPml4[RECURSIVE_SLOT] = PML4_ADDRESS | PRESENT | WRITABLE;
    simPdpt[0] = PAGE_DIRECTORY_ADDRESS | PRESENT | WRITABLE;
    simDirectory[0] = PRESENT | WRITABLE | PAGE_SIZE_FLAG;
    invalidations = 0;
    temporaryActive = false;
    nxEnabled = true;
    activeRootPhysical = PML4_ADDRESS;
    initialised = true;
    return true;
#else
    const volatile uint64_t* bootPml4 =
        reinterpret_cast<volatile const uint64_t*>(PML4_ADDRESS);
    const volatile uint64_t* bootPdpt =
        reinterpret_cast<volatile const uint64_t*>(PDPT_ADDRESS);
    const volatile uint64_t* bootDirectory =
        reinterpret_cast<volatile const uint64_t*>(PAGE_DIRECTORY_ADDRESS);

    if ((bootPml4[0] & (PRESENT | ADDRESS_4K_MASK)) !=
            (PRESENT | PDPT_ADDRESS) ||
        (bootPdpt[0] & (PRESENT | ADDRESS_4K_MASK)) !=
            (PRESENT | PAGE_DIRECTORY_ADDRESS) ||
        (bootDirectory[0] & (PRESENT | PAGE_SIZE_FLAG)) !=
            (PRESENT | PAGE_SIZE_FLAG) ||
        (bootPml4[RECURSIVE_SLOT] & (PRESENT | ADDRESS_4K_MASK)) !=
            (PRESENT | PML4_ADDRESS))
    {
        initialised = false;
        return false;
    }
    uint32_t eferLow = 0, eferHigh = 0;
    const uint32_t eferMsr = 0xC0000080;
    asm volatile("rdmsr" : "=a"(eferLow), "=d"(eferHigh) : "c"(eferMsr));
    (void)eferHigh;
    nxEnabled = (eferLow & (1u << 11)) != 0;
    uint64_t root = 0;
    asm volatile("mov %%cr3, %0" : "=r"(root));
    activeRootPhysical = root & ADDRESS_4K_MASK;
    temporaryActive = false;
    initialised = true;
    return true;
#endif
}

bool queryPage(uint64_t virtualAddress, PageInfo& info)
{
    if (!initialised || !isCanonical(virtualAddress)) return false;
    const uint16_t a = pml4Index(virtualAddress);
    const uint16_t b = pdptIndex(virtualAddress);
    const uint16_t c = directoryIndex(virtualAddress);
    const uint16_t d = tableIndex(virtualAddress);
    uint64_t* l4 = pml4Table();
    if (!(l4[a] & PRESENT)) return false;
    uint64_t* l3 = pdptTable(a);
    if (!l3 || !(l3[b] & PRESENT)) return false;
    if (l3[b] & PAGE_SIZE_FLAG)
    {
        info.pageSize = ONE_GIB;
        info.physicalAddress = (l3[b] & ONE_GIB_ADDRESS_MASK) |
                               (virtualAddress & (ONE_GIB - 1));
        info.flags = l3[b] & (MAP_FLAGS_MASK | PRESENT);
        return true;
    }
    uint64_t* l2 = directoryTable(a, b);
    if (!l2 || !(l2[c] & PRESENT)) return false;
    if (l2[c] & PAGE_SIZE_FLAG)
    {
        info.pageSize = HUGE_PAGE_SIZE;
        info.physicalAddress = (l2[c] & HUGE_PAGE_ADDRESS_MASK) |
                               (virtualAddress & HUGE_PAGE_OFFSET_MASK);
        info.flags = l2[c] & (MAP_FLAGS_MASK | PRESENT);
        return true;
    }
    uint64_t* l1 = pageTable(a, b, c);
    if (!l1 || !(l1[d] & PRESENT)) return false;
    info.pageSize = PAGE_SIZE;
    info.physicalAddress = (l1[d] & ADDRESS_4K_MASK) |
                           (virtualAddress & PAGE_OFFSET_MASK);
    info.flags = l1[d] & (MAP_FLAGS_MASK | PRESENT);
    return true;
}

namespace {

uint64_t readActiveRoot()
{
#ifdef TALONS_PAGING_HOST_TEST
    return activeRootPhysical;
#else
    uint64_t root = 0;
    asm volatile("mov %%cr3, %0" : "=r"(root));
    return root & ADDRESS_4K_MASK;
#endif
}

bool switchRoot(uint64_t root)
{
    if (!validPhysicalPage(root)) return false;
#ifdef TALONS_PAGING_HOST_TEST
    if (!simTable(root)) return false;
#else
    asm volatile("mov %0, %%cr3" : : "r"(root) : "memory");
#endif
    activeRootPhysical = root;
    return true;
}

bool userAddress(uint64_t address)
{
    return address >= USER_BASE && address < USER_END &&
           isCanonical(address);
}

bool validAddressSpaceRoot(uint64_t rootPhysical)
{
    if (rootPhysical == PML4_ADDRESS) return true;
    if (!validPhysicalPage(rootPhysical) ||
        !talons_frames::isAllocated(rootPhysical)) return false;
    void* mapped = mapTemporaryFrame(rootPhysical);
    if (!mapped) return false;
    const bool valid =
        (static_cast<uint64_t*>(mapped)[RECURSIVE_SLOT] &
         (PRESENT | ADDRESS_4K_MASK)) ==
        (rootPhysical | PRESENT) &&
        (static_cast<uint64_t*>(mapped)[0] &
         (PRESENT | USER_ACCESSIBLE)) == (PRESENT | USER_ACCESSIBLE);
    return unmapTemporaryFrame() && valid;
}

} // namespace

bool createAddressSpace(AddressSpace& space)
{
    space = {0, false};
    if (!initialised || temporaryActive || readActiveRoot() != activeRootPhysical)
        return false;

    uint64_t* sourcePml4 = pml4Table();
    if (!sourcePml4 || !(sourcePml4[0] & PRESENT)) return false;
    uint64_t* sourcePdpt = pdptTable(0);
    if (!sourcePdpt || !(sourcePdpt[0] & PRESENT)) return false;

    const uint64_t rootFrame = talons_frames::allocate();
    if (!rootFrame) return false;
#ifdef TALONS_PAGING_HOST_TEST
    uint64_t* newRoot = registerSimTable(rootFrame);
#else
    uint64_t* newRoot = nullptr;
#endif
    void* rootBytes = mapTemporaryFrame(rootFrame);
    if (!rootBytes)
    {
#ifdef TALONS_PAGING_HOST_TEST
        if (newRoot) unregisterSimTable(rootFrame);
#endif
        talons_frames::release(rootFrame);
        return false;
    }
    newRoot = static_cast<uint64_t*>(rootBytes);
    for (size_t i = 0; i < 512; ++i) newRoot[i] = 0;
    // Preserve the kernel's upper-half mappings; slot 510 is always private.
    for (size_t i = 256; i < 512; ++i)
        if (i != RECURSIVE_SLOT && i != 256) newRoot[i] = sourcePml4[i];
    newRoot[RECURSIVE_SLOT] = rootFrame | PRESENT | WRITABLE;
    if (!unmapTemporaryFrame())
    {
        if (!temporaryActive) talons_frames::release(rootFrame);
        return false;
    }

    const uint64_t pdptFrame = talons_frames::allocate();
    if (!pdptFrame)
    {
        talons_frames::release(rootFrame);
        return false;
    }
#ifdef TALONS_PAGING_HOST_TEST
    uint64_t* newPdpt = registerSimTable(pdptFrame);
#else
    uint64_t* newPdpt = nullptr;
#endif
    void* pdptBytes = mapTemporaryFrame(pdptFrame);
    if (!pdptBytes)
    {
#ifdef TALONS_PAGING_HOST_TEST
        if (newPdpt) unregisterSimTable(pdptFrame);
#endif
        talons_frames::release(pdptFrame);
        talons_frames::release(rootFrame);
        return false;
    }
    newPdpt = static_cast<uint64_t*>(pdptBytes);
    for (size_t i = 0; i < 512; ++i)
        newPdpt[i] = i == 0 ? sourcePdpt[i] : 0;
    if (!unmapTemporaryFrame())
    {
        if (!temporaryActive)
        {
#ifdef TALONS_PAGING_HOST_TEST
            unregisterSimTable(pdptFrame);
            unregisterSimTable(rootFrame);
#endif
            talons_frames::release(pdptFrame);
            talons_frames::release(rootFrame);
        }
        return false;
    }

    // Mark the private root and cloned PDPT user-capable. The only inherited
    // lower-half leaf is PDPT[0], whose existing directory remains supervisor.
    void* newRootMap = mapTemporaryFrame(rootFrame);
    if (!newRootMap) goto fail_space;
    static_cast<uint64_t*>(newRootMap)[0] =
        pdptFrame | PRESENT | WRITABLE | USER_ACCESSIBLE;
    if (!unmapTemporaryFrame()) goto fail_space;
    space = {rootFrame, true};
    return true;

fail_space:
    if (temporaryActive) return false;
#ifdef TALONS_PAGING_HOST_TEST
    unregisterSimTable(pdptFrame);
    unregisterSimTable(rootFrame);
#endif
    talons_frames::release(pdptFrame);
    talons_frames::release(rootFrame);
    return false;
}

bool activateAddressSpace(const AddressSpace& space)
{
    if (!initialised || !space.valid || !space.rootPhysical || temporaryActive)
        return false;
    if (!validAddressSpaceRoot(space.rootPhysical)) return false;
    return switchRoot(space.rootPhysical);
}

uint64_t activeAddressSpace()
{
    return readActiveRoot();
}

bool queryAddressSpace(const AddressSpace& space, uint64_t virtualAddress,
                       PageInfo& info)
{
    if (!space.valid || !isCanonical(virtualAddress) ||
        !validAddressSpaceRoot(space.rootPhysical)) return false;
    const uint64_t previous = readActiveRoot();
    if (!switchRoot(space.rootPhysical)) return false;
    const bool result = queryPage(virtualAddress, info);
    const bool restored = switchRoot(previous);
    return restored && result;
}

bool mapUserPage(AddressSpace& space, uint64_t virtualAddress,
                 uint64_t& physicalAddress, uint64_t flags)
{
    physicalAddress = 0;
    constexpr uint64_t USER_FLAGS = MAP_USER | MAP_WRITABLE |
        MAP_WRITE_THROUGH | MAP_CACHE_DISABLE | MAP_NO_EXECUTE;
    if (!space.valid || !userAddress(virtualAddress) ||
        !(flags & MAP_USER) || !validMapFlags(flags, USER_FLAGS) ||
        !validAddressSpaceRoot(space.rootPhysical)) return false;
    const uint64_t previous = readActiveRoot();
    if (!switchRoot(space.rootPhysical)) return false;
    userMappingInProgress = true;
    const bool result = mapNewPage(virtualAddress, physicalAddress, flags);
    userMappingInProgress = false;
    const bool restored = switchRoot(previous);
    if (!restored && result) return false;
    return result && restored;
}

bool unmapUserPage(AddressSpace& space, uint64_t virtualAddress)
{
    if (!space.valid || !userAddress(virtualAddress) ||
        !isPageAligned(virtualAddress) ||
        !validAddressSpaceRoot(space.rootPhysical)) return false;
    const uint64_t previous = readActiveRoot();
    if (!switchRoot(space.rootPhysical)) return false;
    PageInfo info{};
    const bool found = queryPage(virtualAddress, info) &&
        info.pageSize == PAGE_SIZE && (info.flags & MAP_USER);
    userMappingInProgress = true;
    const bool unmapped = found && unmapPage(virtualAddress);
    userMappingInProgress = false;
    bool released = false;
    if (unmapped) released = talons_frames::release(
        info.physicalAddress & ~(PAGE_SIZE - 1));
    const bool restored = switchRoot(previous);
    return unmapped && released && restored;
}

bool destroyAddressSpace(AddressSpace& space)
{
    if (!initialised || !space.valid || !space.rootPhysical || temporaryActive ||
        !validAddressSpaceRoot(space.rootPhysical))
        return false;
    const uint64_t originalRoot = readActiveRoot();
    if (originalRoot == space.rootPhysical &&
        !switchRoot(PML4_ADDRESS)) return false;

    // A private lower-half PDPT is attached at root[0]. Entry 0 is the shared
    // supervisor identity map and is never traversed or reclaimed here.
    uint64_t* root = nullptr;
    uint64_t pdptFrame = 0;
    void* rootMap = mapTemporaryFrame(space.rootPhysical);
    if (!rootMap) goto destroy_fail_restore;
    root = static_cast<uint64_t*>(rootMap);
    pdptFrame = root[0] & ADDRESS_4K_MASK;
    if (!unmapTemporaryFrame()) goto destroy_fail_restore;
    if (!talons_frames::isAllocated(pdptFrame)) goto destroy_fail_restore;
    {
        void* pdptMap = mapTemporaryFrame(pdptFrame);
        if (!pdptMap) goto destroy_fail_restore;
        uint64_t pdptEntries[512];
        for (size_t i = 0; i < 512; ++i)
            pdptEntries[i] = static_cast<uint64_t*>(pdptMap)[i];
        if (!unmapTemporaryFrame()) goto destroy_fail_restore;

        // Validate the complete owned tree before releasing anything. This
        // makes corrupt or externally modified roots fail without a partial
        // teardown or a double release on a later retry.
        for (size_t b = 1; b < 512; ++b)
        {
            if (!(pdptEntries[b] & PRESENT)) continue;
            if ((pdptEntries[b] & (PAGE_SIZE_FLAG | USER_ACCESSIBLE)) !=
                USER_ACCESSIBLE) goto destroy_fail_restore;
            const uint64_t dirFrame = pdptEntries[b] & ADDRESS_4K_MASK;
            if (!talons_frames::isAllocated(dirFrame))
                goto destroy_fail_restore;
            void* dirMap = mapTemporaryFrame(dirFrame);
            if (!dirMap) goto destroy_fail_restore;
            uint64_t directoryEntries[512];
            for (size_t i = 0; i < 512; ++i)
                directoryEntries[i] = static_cast<uint64_t*>(dirMap)[i];
            if (!unmapTemporaryFrame()) goto destroy_fail_restore;
            for (size_t c = 0; c < 512; ++c)
            {
                if (!(directoryEntries[c] & PRESENT)) continue;
                if ((directoryEntries[c] &
                     (PAGE_SIZE_FLAG | USER_ACCESSIBLE)) != USER_ACCESSIBLE)
                    goto destroy_fail_restore;
                const uint64_t tableFrame =
                    directoryEntries[c] & ADDRESS_4K_MASK;
                if (!talons_frames::isAllocated(tableFrame))
                    goto destroy_fail_restore;
                void* tableMap = mapTemporaryFrame(tableFrame);
                if (!tableMap) goto destroy_fail_restore;
                bool framesOwned = true;
                for (size_t d = 0; d < 512; ++d)
                {
                    const uint64_t entry =
                        static_cast<uint64_t*>(tableMap)[d];
                    if ((entry & PRESENT) &&
                        (!(entry & USER_ACCESSIBLE) ||
                         !talons_frames::isAllocated(entry & ADDRESS_4K_MASK)))
                        framesOwned = false;
                }
                if (!unmapTemporaryFrame() || !framesOwned)
                    goto destroy_fail_restore;
            }
        }

        for (size_t b = 1; b < 512; ++b)
        {
            if (!(pdptEntries[b] & PRESENT)) continue;
            if (pdptEntries[b] & PAGE_SIZE_FLAG) goto destroy_fail_restore;
            const uint64_t dirFrame = pdptEntries[b] & ADDRESS_4K_MASK;
            void* dirMap = mapTemporaryFrame(dirFrame);
            if (!dirMap) goto destroy_fail_restore;
            uint64_t directoryEntries[512];
            for (size_t i = 0; i < 512; ++i)
                directoryEntries[i] = static_cast<uint64_t*>(dirMap)[i];
            if (!unmapTemporaryFrame()) goto destroy_fail_restore;
            for (size_t c = 0; c < 512; ++c)
            {
                if (!(directoryEntries[c] & PRESENT)) continue;
                if (directoryEntries[c] & PAGE_SIZE_FLAG)
                    goto destroy_fail_restore;
                const uint64_t tableFrame = directoryEntries[c] & ADDRESS_4K_MASK;
                void* tableMap = mapTemporaryFrame(tableFrame);
                if (!tableMap) goto destroy_fail_restore;
                uint64_t* table = static_cast<uint64_t*>(tableMap);
                uint64_t dataFrames[512];
                size_t dataCount = 0;
                for (size_t d = 0; d < 512; ++d)
                {
                    if (!(table[d] & PRESENT)) continue;
                    dataFrames[dataCount++] = table[d] & ADDRESS_4K_MASK;
                    table[d] = 0;
                }
                if (!unmapTemporaryFrame()) goto destroy_fail_restore;
                for (size_t d = 0; d < dataCount; ++d)
                    if (!talons_frames::release(dataFrames[d]))
                        goto destroy_fail_restore;

                void* dirUpdateOne = mapTemporaryFrame(dirFrame);
                if (!dirUpdateOne) goto destroy_fail_restore;
                static_cast<uint64_t*>(dirUpdateOne)[c] = 0;
                if (!unmapTemporaryFrame()) goto destroy_fail_restore;
                if (!talons_frames::release(tableFrame))
                    goto destroy_fail_restore;
            }

            void* pdptUpdateOne = mapTemporaryFrame(pdptFrame);
            if (!pdptUpdateOne) goto destroy_fail_restore;
            static_cast<uint64_t*>(pdptUpdateOne)[b] = 0;
            if (!unmapTemporaryFrame()) goto destroy_fail_restore;
            if (!talons_frames::release(dirFrame)) goto destroy_fail_restore;
        }
    }
    if (!talons_frames::release(pdptFrame) ||
        !talons_frames::release(space.rootPhysical)) goto destroy_fail_restore;
#ifdef TALONS_PAGING_HOST_TEST
    unregisterSimTable(pdptFrame);
    unregisterSimTable(space.rootPhysical);
#endif
    if (originalRoot == space.rootPhysical) switchRoot(PML4_ADDRESS);
    space = {0, false};
    return true;

destroy_fail_restore:
    if (temporaryActive) unmapTemporaryFrame();
    if (originalRoot != space.rootPhysical) switchRoot(originalRoot);
    return false;
}

bool translate(uint64_t virtualAddress, uint64_t& physicalAddress)
{
    PageInfo info{};
    if (!queryPage(virtualAddress, info)) return false;
    physicalAddress = info.physicalAddress;
    return true;
}

bool mapPage(uint64_t virtualAddress, uint64_t physicalAddress,
             uint64_t flags)
{
    if (!initialised || !isCanonical(virtualAddress) ||
        !isPageAligned(virtualAddress) ||
        !validPhysicalPage(physicalAddress) ||
        !validMapFlags(flags, MAP_FLAGS_MASK) ||
        ((flags & MAP_NO_EXECUTE) && !nxEnabled) ||
        ((flags & MAP_USER) && !userMappingInProgress) ||
        ((flags & MAP_USER) &&
         (virtualAddress < USER_BASE || virtualAddress >= USER_END)) ||
        (activeRootPhysical != PML4_ADDRESS &&
         virtualAddress >= USER_BASE && virtualAddress < USER_END &&
         !userMappingInProgress) ||
        pml4Index(virtualAddress) == RECURSIVE_SLOT)
        return false;

    const uint16_t a = pml4Index(virtualAddress);
    const uint16_t b = pdptIndex(virtualAddress);
    const uint16_t c = directoryIndex(virtualAddress);
    const uint16_t d = tableIndex(virtualAddress);
    uint64_t* l4 = pml4Table();

    // Existing 1 GiB, 2 MiB and 4 KiB mappings are all duplicates. Do
    // this check before allocating or changing any page-table entries.
    uint64_t* l3 = (l4[a] & PRESENT) ? pdptTable(a) : nullptr;
    if (l3 && (l3[b] & PRESENT))
    {
        if (l3[b] & PAGE_SIZE_FLAG) return false;
        uint64_t* l2 = directoryTable(a, b);
        if (l2 && (l2[c] & PRESENT))
        {
            if (l2[c] & PAGE_SIZE_FLAG) return false;
            uint64_t* l1 = pageTable(a, b, c);
            if (!l1 || (l1[d] & PRESENT)) return false;
        }
    }

    CreatedTable created[3]{};
    size_t createdCount = 0;
    if (!(l4[a] & PRESENT))
    {
        const uint64_t recursive = recursiveTableAddress(3, a, 0, 0);
        uint64_t frame = 0;
        if (!createTable(l4[a], recursive, frame)) return false;
        created[createdCount++] = {&l4[a], frame, recursive};
    }
    l3 = pdptTable(a);
    if (!l3) { rollbackTables(created, createdCount); return false; }

    if (!(l3[b] & PRESENT))
    {
        const uint64_t recursive = recursiveTableAddress(2, a, b, 0);
        uint64_t frame = 0;
        if (!createTable(l3[b], recursive, frame))
        { rollbackTables(created, createdCount); return false; }
        created[createdCount++] = {&l3[b], frame, recursive};
    }
    uint64_t* l2 = directoryTable(a, b);
    if (!l2) { rollbackTables(created, createdCount); return false; }

    if (!(l2[c] & PRESENT))
    {
        const uint64_t recursive = recursiveTableAddress(1, a, b, c);
        uint64_t frame = 0;
        if (!createTable(l2[c], recursive, frame))
        { rollbackTables(created, createdCount); return false; }
        created[createdCount++] = {&l2[c], frame, recursive};
    }
    uint64_t* l1 = pageTable(a, b, c);
    if (!l1 || (l1[d] & PRESENT))
    { rollbackTables(created, createdCount); return false; }

    // Writable and user bits in ancestor entries are permissions ceilings;
    // leaf entries still decide which individual pages receive access.
    l4[a] |= WRITABLE;
    l3[b] |= WRITABLE;
    l2[c] |= WRITABLE;
    if (flags & MAP_USER)
    {
        l4[a] |= USER_ACCESSIBLE;
        l3[b] |= USER_ACCESSIBLE;
        l2[c] |= USER_ACCESSIBLE;
    }
    l1[d] = physicalAddress | PRESENT | flags;
    invalidatePage(virtualAddress);
    return true;
}

bool unmapPage(uint64_t virtualAddress)
{
    if (!initialised || !isCanonical(virtualAddress) ||
        !isPageAligned(virtualAddress) ||
        (activeRootPhysical != PML4_ADDRESS &&
         virtualAddress >= USER_BASE && virtualAddress < USER_END &&
         !userMappingInProgress) ||
        pml4Index(virtualAddress) == RECURSIVE_SLOT)
        return false;

    const uint16_t a = pml4Index(virtualAddress);
    const uint16_t b = pdptIndex(virtualAddress);
    const uint16_t c = directoryIndex(virtualAddress);
    const uint16_t d = tableIndex(virtualAddress);
    uint64_t* l4 = pml4Table();
    if (!(l4[a] & PRESENT)) return false;
    uint64_t* l3 = pdptTable(a);
    if (!l3 || !(l3[b] & PRESENT)) return false;

    uint64_t splitFrame = 0;
    if (l3[b] & PAGE_SIZE_FLAG)
    {
        if (!splitOneGiB(l3[b], a, b, splitFrame)) return false;
        invalidatePage(virtualAddress);
    }
    uint64_t* l2 = directoryTable(a, b);
    if (!l2 || !(l2[c] & PRESENT)) return false;
    if (l2[c] & PAGE_SIZE_FLAG)
    {
        if (!splitTwoMiB(l2[c], a, b, c, splitFrame)) return false;
        invalidatePage(virtualAddress);
    }
    uint64_t* l1 = pageTable(a, b, c);
    if (!l1 || !(l1[d] & PRESENT)) return false;

    l1[d] = 0;
    invalidatePage(virtualAddress);

    if (tableEmpty(l1))
    {
        const uint64_t tableFrame = l2[c] & ADDRESS_4K_MASK;
        reclaimTable(l2[c], tableFrame,
                     recursiveTableAddress(1, a, b, c));
        if (tableEmpty(l2))
        {
            const uint64_t directoryFrame = l3[b] & ADDRESS_4K_MASK;
            reclaimTable(l3[b], directoryFrame,
                         recursiveTableAddress(2, a, b, 0));
            if (tableEmpty(l3))
            {
                const uint64_t pdptFrame = l4[a] & ADDRESS_4K_MASK;
                reclaimTable(l4[a], pdptFrame,
                             recursiveTableAddress(3, a, 0, 0));
            }
        }
    }
    return true;
}

bool mapNewPage(uint64_t virtualAddress, uint64_t& physicalAddress,
                uint64_t flags)
{
    physicalAddress = 0;
    if (!initialised || !isCanonical(virtualAddress) ||
        !isPageAligned(virtualAddress) ||
        pml4Index(virtualAddress) == RECURSIVE_SLOT ||
        !validMapFlags(flags, MAP_FLAGS_MASK) ||
        ((flags & MAP_NO_EXECUTE) && !nxEnabled))
        return false;

    physicalAddress = talons_frames::allocate();
    if (!physicalAddress) return false;
    void* mapped = mapTemporaryFrame(physicalAddress);
    if (!mapped)
    {
        talons_frames::release(physicalAddress);
        physicalAddress = 0;
        return false;
    }
    volatile uint8_t* bytes = static_cast<volatile uint8_t*>(mapped);
    for (size_t i = 0; i < PAGE_SIZE; ++i) bytes[i] = 0;
    if (!unmapTemporaryFrame())
    {
        if (!temporaryActive) talons_frames::release(physicalAddress);
        physicalAddress = 0;
        return false;
    }
    if (!mapPage(virtualAddress, physicalAddress, flags))
    {
        talons_frames::release(physicalAddress);
        physicalAddress = 0;
        return false;
    }
    return true;
}

void* mapTemporaryFrame(uint64_t physicalAddress)
{
    if (!initialised || temporaryActive ||
        !validPhysicalPage(physicalAddress))
        return nullptr;
    const uint64_t tempFlags = MAP_WRITABLE |
        (nxEnabled ? MAP_NO_EXECUTE : 0);
    if (!mapPage(TEMPORARY_WINDOW, physicalAddress, tempFlags))
        return nullptr;
    temporaryActive = true;
#ifdef TALONS_PAGING_HOST_TEST
    uint64_t* table = simTable(physicalAddress);
    if (table) return table;
    uint8_t* bytes = simFrame(physicalAddress);
    if (!bytes)
    {
        if (unmapPage(TEMPORARY_WINDOW)) temporaryActive = false;
        return nullptr;
    }
    return bytes;
#else
    return reinterpret_cast<void*>(TEMPORARY_WINDOW);
#endif
}

bool unmapTemporaryFrame()
{
    if (!initialised || !temporaryActive) return false;
    const bool result = unmapPage(TEMPORARY_WINDOW);
    if (result)
    {
        temporaryActive = false;
    }
    return result;
}

uint64_t allocateZeroedFrame()
{
    const uint64_t frame = talons_frames::allocate();
    if (!frame) return 0;
    void* mapped = mapTemporaryFrame(frame);
    if (!mapped)
    {
        talons_frames::release(frame);
        return 0;
    }
    volatile uint8_t* bytes = static_cast<volatile uint8_t*>(mapped);
    for (size_t i = 0; i < PAGE_SIZE; ++i) bytes[i] = 0;
    if (!unmapTemporaryFrame())
    {
        if (!temporaryActive) talons_frames::release(frame);
        return 0;
    }
    return frame;
}

bool isInitialised() { return initialised; }

namespace testing {

uint64_t invalidationCount()
{
#ifdef TALONS_PAGING_HOST_TEST
    return invalidations;
#else
    return 0;
#endif
}

void resetInvalidationCount()
{
#ifdef TALONS_PAGING_HOST_TEST
    invalidations = 0;
#endif
}

bool install1GiBMapping(uint64_t virtualAddress, uint64_t physicalAddress,
                        uint64_t flags)
{
#ifdef TALONS_PAGING_HOST_TEST
    if (!initialised || !isCanonical(virtualAddress) ||
        !validMapFlags(flags, MAP_FLAGS_MASK) ||
        (virtualAddress & (ONE_GIB - 1)) ||
        (physicalAddress & (ONE_GIB - 1)) ||
        (physicalAddress & ~ONE_GIB_ADDRESS_MASK)) return false;
    const uint16_t a = pml4Index(virtualAddress);
    const uint16_t b = pdptIndex(virtualAddress);
    if (a == RECURSIVE_SLOT) return false;
    uint64_t* l4 = pml4Table();
    if (!(l4[a] & PRESENT))
    {
        uint64_t frame = 0;
        if (!createTable(l4[a], recursiveTableAddress(3, a, 0, 0), frame))
            return false;
    }
    uint64_t* l3 = pdptTable(a);
    if (!l3 || (l3[b] & PRESENT)) return false;
    l3[b] = physicalAddress | PRESENT | PAGE_SIZE_FLAG | flags;
    invalidatePage(virtualAddress);
    return true;
#else
    (void)virtualAddress; (void)physicalAddress; (void)flags;
    return false;
#endif
}

} // namespace testing
} // namespace talons_paging
