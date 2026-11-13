#pragma once

// Small allocation-free text formatting helpers for memory reports
// (Phase 5, M1). Header-only, host-testable. Every function writes a
// NUL-terminated string into 'out' and returns its length, or returns 0
// (leaving out[0] == '\0' when cap > 0) if the buffer is too small.

#include <stddef.h>
#include <stdint.h>

namespace talons_memfmt
{
    inline size_t formatDecimal(uint64_t value, char* out, size_t cap)
    {
        if (out == nullptr || cap == 0)
            return 0;

        char digits[20];
        size_t count = 0;

        do
        {
            digits[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);

        if (count + 1 > cap)
        {
            out[0] = '\0';
            return 0;
        }

        for (size_t i = 0; i < count; ++i)
            out[i] = digits[count - 1 - i];

        out[count] = '\0';
        return count;
    }

    // "0x" followed by at least 'minDigits' hex digits (1..16), upper
    // case, zero padded. Never truncates the value.
    inline size_t formatHex(
        uint64_t value, unsigned minDigits, char* out, size_t cap)
    {
        if (out == nullptr || cap == 0)
            return 0;

        if (minDigits < 1)
            minDigits = 1;
        if (minDigits > 16)
            minDigits = 16;

        unsigned needed = 1;
        for (uint64_t v = value >> 4; v != 0; v >>= 4)
            ++needed;

        const unsigned width = needed > minDigits ? needed : minDigits;

        if (static_cast<size_t>(width) + 3 > cap)
        {
            out[0] = '\0';
            return 0;
        }

        out[0] = '0';
        out[1] = 'x';

        for (unsigned i = 0; i < width; ++i)
        {
            const unsigned shift = (width - 1 - i) * 4;
            const unsigned nibble =
                static_cast<unsigned>((value >> shift) & 0xF);
            out[2 + i] = static_cast<char>(
                nibble < 10 ? '0' + nibble : 'A' + (nibble - 10));
        }

        out[2 + width] = '\0';
        return 2 + width;
    }

    // "N bytes" below 1 KiB, otherwise "N KiB" / "N MiB" / "N GiB" using
    // the largest unit that gives N >= 1. The value is truncated, not
    // rounded, so it never overstates the amount of memory.
    inline size_t formatSize(uint64_t bytes, char* out, size_t cap)
    {
        const char* unit = " bytes";
        uint64_t value = bytes;

        if (bytes >= (1ULL << 30))
        {
            value = bytes >> 30;
            unit = " GiB";
        }
        else if (bytes >= (1ULL << 20))
        {
            value = bytes >> 20;
            unit = " MiB";
        }
        else if (bytes >= (1ULL << 10))
        {
            value = bytes >> 10;
            unit = " KiB";
        }

        const size_t length = formatDecimal(value, out, cap);
        if (length == 0)
            return 0;

        size_t unitLength = 0;
        while (unit[unitLength] != '\0')
            ++unitLength;

        if (length + unitLength + 1 > cap)
        {
            out[0] = '\0';
            return 0;
        }

        for (size_t i = 0; i <= unitLength; ++i)
            out[length + i] = unit[i];

        return length + unitLength;
    }
}
