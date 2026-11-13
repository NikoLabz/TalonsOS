#pragma once

#include <cstdint>

namespace talons_paging {

constexpr uint64_t MAP_WRITABLE = 1ULL << 1;
constexpr uint64_t MAP_USER = 1ULL << 2;
constexpr uint64_t MAP_WRITE_THROUGH = 1ULL << 3;
constexpr uint64_t MAP_CACHE_DISABLE = 1ULL << 4;
constexpr uint64_t MAP_NO_EXECUTE = 1ULL << 63;
constexpr uint64_t MAP_FLAGS_MASK = MAP_WRITABLE | MAP_USER |
                                    MAP_WRITE_THROUGH | MAP_CACHE_DISABLE |
                                    MAP_NO_EXECUTE;

struct PageInfo
{
    uint64_t physicalAddress;
    uint64_t flags;
    uint64_t pageSize;
};

// User virtual range [1 GiB, 512 GiB). The first GiB remains the supervisor
// identity map required by the current boot architecture.
constexpr uint64_t USER_BASE = 1ULL << 30;
constexpr uint64_t USER_END = 1ULL << 39;

struct AddressSpace
{
    uint64_t rootPhysical;
    bool valid;
};

// Initialise the paging inspection layer using the boot page tables.
bool init();

// Translate a virtual address to its physical address.
// Returns false if the address is not mapped.
bool translate(uint64_t virtualAddress, uint64_t& physicalAddress);

// Report whether the paging inspection layer is initialised.
bool isInitialised();

// Map a virtual 4 KiB page to a physical 4 KiB frame.
bool mapPage(uint64_t virtualAddress, uint64_t physicalAddress,
             uint64_t flags = MAP_WRITABLE);

// Allocate a zeroed frame and map it. The caller owns the frame and must
// unmap the page and release the frame when it is no longer needed.
bool mapNewPage(uint64_t virtualAddress, uint64_t& physicalAddress,
                uint64_t flags = MAP_WRITABLE);

bool queryPage(uint64_t virtualAddress, PageInfo& info);

// Independent address spaces share supervisor-only kernel mappings. Creation
// leaves the active root unchanged. Targeted APIs temporarily switch roots and
// restore the previously active root before returning.
bool createAddressSpace(AddressSpace& space);
bool destroyAddressSpace(AddressSpace& space);
bool activateAddressSpace(const AddressSpace& space);
uint64_t activeAddressSpace();
bool mapUserPage(AddressSpace& space, uint64_t virtualAddress,
                 uint64_t& physicalAddress,
                 uint64_t flags = MAP_WRITABLE | MAP_NO_EXECUTE);
bool unmapUserPage(AddressSpace& space, uint64_t virtualAddress);
bool queryAddressSpace(const AddressSpace& space, uint64_t virtualAddress,
                       PageInfo& info);

// Remove a virtual 4 KiB mapping.
bool unmapPage(uint64_t virtualAddress);

// Map a physical frame into the temporary mapping window.
void* mapTemporaryFrame(uint64_t physicalAddress);

// Remove the temporary mapping.
bool unmapTemporaryFrame();

// Allocate a physical frame, clear it through the temporary window, and
// return 0 on failure. Requires initialized paging and the frame allocator.
uint64_t allocateZeroedFrame();

namespace testing {
uint64_t invalidationCount();
void resetInvalidationCount();
bool install1GiBMapping(uint64_t virtualAddress, uint64_t physicalAddress,
                        uint64_t flags);
}

}
