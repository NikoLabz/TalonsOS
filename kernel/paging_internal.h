#pragma once

#include <cstdint>

namespace talons_paging_internal {

constexpr uint64_t PAGE_SIZE = 4096;
constexpr uint64_t PAGE_OFFSET_MASK = PAGE_SIZE - 1;

constexpr uint64_t HUGE_PAGE_SIZE = 2ULL * 1024 * 1024;
constexpr uint64_t HUGE_PAGE_OFFSET_MASK = HUGE_PAGE_SIZE - 1;
constexpr uint64_t HUGE_PAGE_ADDRESS_MASK = 0x000FFFFFFFE00000ULL;

constexpr uint64_t GIGA_PAGE_SIZE = 1ULL << 30;
constexpr uint64_t GIGA_PAGE_OFFSET_MASK = GIGA_PAGE_SIZE - 1;
constexpr uint64_t GIGA_PAGE_ADDRESS_MASK = 0x000FFFFFC0000000ULL;

constexpr uint64_t PRESENT = 1ULL << 0;
constexpr uint64_t WRITABLE = 1ULL << 1;
constexpr uint64_t USER_ACCESSIBLE = 1ULL << 2;
constexpr uint64_t WRITE_THROUGH = 1ULL << 3;
constexpr uint64_t CACHE_DISABLE = 1ULL << 4;
constexpr uint64_t PAGE_SIZE_FLAG = 1ULL << 7;
constexpr uint64_t NO_EXECUTE = 1ULL << 63;
constexpr uint64_t ADDRESS_4K_MASK = 0x000FFFFFFFFFF000ULL;

constexpr uint16_t RECURSIVE_INDEX = 510;

constexpr uint16_t pml4Index(uint64_t address)
{
    return (address >> 39) & 0x1FF;
}

constexpr uint16_t pdptIndex(uint64_t address)
{
    return (address >> 30) & 0x1FF;
}

constexpr uint16_t directoryIndex(uint64_t address)
{
    return (address >> 21) & 0x1FF;
}

constexpr uint16_t tableIndex(uint64_t address)
{
    return (address >> 12) & 0x1FF;
}

constexpr bool isPageAligned(uint64_t address)
{
    return (address & PAGE_OFFSET_MASK) == 0;
}

constexpr bool isCanonical(uint64_t address)
{
    const uint64_t upper = address >> 48;
    const bool signBit = (address & (1ULL << 47)) != 0;

    return signBit ? upper == 0xFFFF : upper == 0;
}

// Constructs a canonical recursive-mapping virtual address.
constexpr uint64_t recursiveAddress(
    uint16_t a,
    uint16_t b,
    uint16_t c,
    uint16_t d)
{
    uint64_t address =
        (uint64_t(a) << 39) |
        (uint64_t(b) << 30) |
        (uint64_t(c) << 21) |
        (uint64_t(d) << 12);

    if (address & (1ULL << 47))
        address |= 0xFFFF000000000000ULL;

    return address;
}

constexpr uint64_t recursivePml4Address()
{
    return recursiveAddress(
        RECURSIVE_INDEX,
        RECURSIVE_INDEX,
        RECURSIVE_INDEX,
        RECURSIVE_INDEX);
}

constexpr uint64_t recursivePdptAddress(uint16_t a)
{
    return recursiveAddress(
        RECURSIVE_INDEX,
        RECURSIVE_INDEX,
        RECURSIVE_INDEX,
        a);
}

constexpr uint64_t recursiveDirectoryAddress(uint16_t a, uint16_t b)
{
    return recursiveAddress(
        RECURSIVE_INDEX,
        RECURSIVE_INDEX,
        a,
        b);
}

constexpr uint64_t recursivePageTableAddress(
    uint16_t a,
    uint16_t b,
    uint16_t c)
{
    return recursiveAddress(
        RECURSIVE_INDEX,
        a,
        b,
        c);
}

// Translate a 2 MiB page-directory entry.
inline bool translateHugePage(
    uint64_t virtualAddress,
    uint64_t entry,
    uint64_t& physicalAddress)
{
    if (!(entry & PRESENT) || !(entry & PAGE_SIZE_FLAG))
        return false;

    physicalAddress =
        (entry & HUGE_PAGE_ADDRESS_MASK) |
        (virtualAddress & HUGE_PAGE_OFFSET_MASK);

    return true;
}

// Translate a 1 GiB page-directory-pointer entry.
inline bool translateGigaPage(
    uint64_t virtualAddress,
    uint64_t entry,
    uint64_t& physicalAddress)
{
    if (!(entry & PRESENT) || !(entry & PAGE_SIZE_FLAG))
        return false;

    physicalAddress =
        (entry & GIGA_PAGE_ADDRESS_MASK) |
        (virtualAddress & GIGA_PAGE_OFFSET_MASK);

    return true;
}

constexpr bool validPhysicalPage(uint64_t address)
{
    return isPageAligned(address) &&
           (address & ~(ADDRESS_4K_MASK | PAGE_OFFSET_MASK)) == 0;
}

constexpr bool validMapFlags(uint64_t flags, uint64_t allowed)
{
    return (flags & ~allowed) == 0;
}

// Convert a 1 GiB page entry into 512 2 MiB page entries.
inline bool split1GiBEntry(
    uint64_t entry,
    uint64_t* directory)
{
    if (!directory ||
        !(entry & PRESENT) ||
        !(entry & PAGE_SIZE_FLAG))
        return false;

    const uint64_t base = entry & GIGA_PAGE_ADDRESS_MASK;

    // Preserve permissions and cache flags, excluding the 1 GiB
    // physical address and huge-page indicator.
    const uint64_t flags = entry &
        ~(GIGA_PAGE_ADDRESS_MASK | PAGE_SIZE_FLAG);

    for (uint64_t i = 0; i < 512; ++i) {
        directory[i] = (base + i * HUGE_PAGE_SIZE) |
                       flags | PAGE_SIZE_FLAG;
    }

    return true;
}

// Convert a 2 MiB page entry into 512 4 KiB page entries.
inline bool split2MiBEntry(
    uint64_t entry,
    uint64_t* table)
{
    if (!table ||
        !(entry & PRESENT) ||
        !(entry & PAGE_SIZE_FLAG))
        return false;

    const uint64_t base = entry & HUGE_PAGE_ADDRESS_MASK;

    uint64_t flags = entry & ~HUGE_PAGE_ADDRESS_MASK;

    // Huge-page PAT is bit 12. Normal PTE PAT is bit 7.
    const bool pat = (flags & (1ULL << 12)) != 0;

    flags &= ~((1ULL << 12) | PAGE_SIZE_FLAG);

    if (pat)
        flags |= PAGE_SIZE_FLAG;

    for (uint64_t i = 0; i < 512; ++i) {
        table[i] =
            (base + i * PAGE_SIZE) |
            flags;
    }

    return true;
}

} // namespace talons_paging_internal
