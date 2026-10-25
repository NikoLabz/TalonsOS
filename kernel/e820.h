#pragma once

#include <stdint.h>

namespace talons_e820
{
    constexpr uint32_t MAGIC = 0x304D454D;
    constexpr uint32_t MAX_ENTRIES = 64;
    constexpr uint32_t ENTRY_SIZE = 24;
    constexpr uint32_t TYPE_USABLE = 1;

    struct Entry
    {
        uint64_t base;
        uint64_t length;
        uint32_t type;
        uint32_t attributes;
    };

    struct Header
    {
        uint32_t magic;
        uint32_t count;
        uint32_t entry_size;
        uint32_t reserved;
    };

    static_assert(sizeof(Entry) == ENTRY_SIZE,
                  "E820 entry must be 24 bytes");
    static_assert(sizeof(Header) == 16,
                  "E820 header must be 16 bytes");

    inline bool validHeader(const Header& header)
    {
        return header.magic == MAGIC &&
               header.count > 0 &&
               header.count <= MAX_ENTRIES &&
               header.entry_size == ENTRY_SIZE;
    }

inline bool validMap(const Header& header, const Entry* entries)
{
    if (!validHeader(header) || entries == nullptr)
        return false;

    for (uint32_t i = 0; i < header.count; ++i)
    {
        const Entry& entry = entries[i];

        // Ignore empty entries, but reject overflowing address ranges.
        if (entry.length != 0 &&
            entry.base > UINT64_MAX - entry.length)
        {
            return false;
        }
    }

    return true;
}

    inline bool isUsable(const Entry& entry)
    {
        if (entry.type != TYPE_USABLE || entry.length == 0)
            return false;

        if (entry.base > UINT64_MAX - entry.length)
            return false;

        // Bit 0 indicates whether a normalised E820 entry is enabled.
        if ((entry.attributes & 1u) == 0)
            return false;

        return true;
    }

    // Intersect an E820 entry with a requested address range.
    // Both ranges use half-open intervals: [base, end).
    inline bool usableIntersection(
        const Entry& entry,
        uint64_t rangeBase,
        uint64_t rangeLength,
        uint64_t& resultBase,
        uint64_t& resultLength)
    {
        resultBase = 0;
        resultLength = 0;

        if (!isUsable(entry) || rangeLength == 0)
            return false;

        if (rangeBase > UINT64_MAX - rangeLength)
            return false;

        const uint64_t entryEnd = entry.base + entry.length;
        const uint64_t rangeEnd = rangeBase + rangeLength;

        const uint64_t start =
            entry.base > rangeBase ? entry.base : rangeBase;

        const uint64_t end =
            entryEnd < rangeEnd ? entryEnd : rangeEnd;

        if (start >= end)
            return false;

        resultBase = start;
        resultLength = end - start;
        return true;
    }

}
bool e820_init();
bool e820_is_valid();
