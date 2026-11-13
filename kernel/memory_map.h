#pragma once

// Physical memory map summary (Phase 5, M1).
//
// Pure functions over the BIOS E820 map that stage2.asm leaves at
// 0x5FF0 (header) / 0x6000 (entries). Header-only and free of hardware
// access so it can be unit-tested on the host, like e820.h.

#include "e820.h"

#include <stdint.h>

namespace talons_memmap
{
    // Standard E820 type codes (type 1 is talons_e820::TYPE_USABLE).
    constexpr uint32_t TYPE_USABLE = 1;
    constexpr uint32_t TYPE_RESERVED = 2;
    constexpr uint32_t TYPE_ACPI_RECLAIMABLE = 3;
    constexpr uint32_t TYPE_ACPI_NVS = 4;
    constexpr uint32_t TYPE_BAD_MEMORY = 5;

    // Totals are raw sums of entry lengths. If hasOverlap is true the
    // BIOS reported overlapping ranges and the totals may double count;
    // frame_allocator treats such frames as unusable.
    struct Summary
    {
        uint32_t entryCount;       // header.count
        uint32_t emptyEntries;     // length == 0
        uint32_t usableEntries;    // enabled, type 1, non-empty
        uint32_t reservedEntries;  // enabled, other type, non-empty
        uint32_t disabledEntries;  // attribute bit 0 clear, non-empty
        uint64_t usableBytes;
        uint64_t reservedBytes;
        uint64_t disabledBytes;
        uint64_t highestEnd;       // exclusive end of highest enabled entry
        uint64_t highestUsableEnd; // exclusive end of highest usable entry
        bool hasOverlap;           // any two non-empty entries overlap
    };

    inline const char* typeName(uint32_t type)
    {
        switch (type)
        {
        case TYPE_USABLE:           return "Usable";
        case TYPE_RESERVED:         return "Reserved";
        case TYPE_ACPI_RECLAIMABLE: return "ACPI reclaimable";
        case TYPE_ACPI_NVS:         return "ACPI NVS";
        case TYPE_BAD_MEMORY:       return "Bad memory";
        default:                    return "Unknown";
        }
    }

    inline uint64_t saturatingAdd(uint64_t a, uint64_t b)
    {
        return a > UINT64_MAX - b ? UINT64_MAX : a + b;
    }

    // Fills 'out'. Returns false (and zeroes 'out') if the map is not a
    // valid E820 map according to talons_e820::validMap().
    inline bool summarise(
        const talons_e820::Header& header,
        const talons_e820::Entry* entries,
        Summary& out)
    {
        out = Summary{};

        if (!talons_e820::validMap(header, entries))
            return false;

        out.entryCount = header.count;

        for (uint32_t i = 0; i < header.count; ++i)
        {
            const talons_e820::Entry& entry = entries[i];

            if (entry.length == 0)
            {
                ++out.emptyEntries;
                continue;
            }

            // validMap() already rejected base + length overflow.
            const uint64_t end = entry.base + entry.length;

            for (uint32_t j = i + 1; j < header.count; ++j)
            {
                const talons_e820::Entry& other = entries[j];

                if (other.length == 0)
                    continue;

                if (entry.base < other.base + other.length &&
                    other.base < end)
                    out.hasOverlap = true;
            }

            if ((entry.attributes & 1u) == 0)
            {
                ++out.disabledEntries;
                out.disabledBytes =
                    saturatingAdd(out.disabledBytes, entry.length);
                continue;
            }

            if (end > out.highestEnd)
                out.highestEnd = end;

            if (entry.type == TYPE_USABLE)
            {
                ++out.usableEntries;
                out.usableBytes =
                    saturatingAdd(out.usableBytes, entry.length);

                if (end > out.highestUsableEnd)
                    out.highestUsableEnd = end;
            }
            else
            {
                ++out.reservedEntries;
                out.reservedBytes =
                    saturatingAdd(out.reservedBytes, entry.length);
            }
        }

        return true;
    }
}
