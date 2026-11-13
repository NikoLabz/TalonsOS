#include "memory_format.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace talons_memfmt;

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
    char buffer[64];

    // Decimal.
    assert(formatDecimal(0, buffer, sizeof(buffer)) == 1);
    assert(equal(buffer, "0"));
    assert(formatDecimal(7, buffer, sizeof(buffer)) == 1);
    assert(equal(buffer, "7"));
    assert(formatDecimal(1234567890, buffer, sizeof(buffer)) == 10);
    assert(equal(buffer, "1234567890"));
    assert(formatDecimal(UINT64_MAX, buffer, sizeof(buffer)) == 20);
    assert(equal(buffer, "18446744073709551615"));

    // Decimal: exact-fit and too-small buffers.
    char tiny[4];
    assert(formatDecimal(123, tiny, sizeof(tiny)) == 3);
    assert(equal(tiny, "123"));
    assert(formatDecimal(1234, tiny, sizeof(tiny)) == 0);
    assert(tiny[0] == '\0');
    assert(formatDecimal(1, nullptr, 8) == 0);
    assert(formatDecimal(1, buffer, 0) == 0);

    // Hex.
    assert(formatHex(0, 1, buffer, sizeof(buffer)) == 3);
    assert(equal(buffer, "0x0"));
    assert(formatHex(0, 0, buffer, sizeof(buffer)) == 3);
    assert(equal(buffer, "0x0"));
    assert(formatHex(0xDEADBEEF, 16, buffer, sizeof(buffer)) == 18);
    assert(equal(buffer, "0x00000000DEADBEEF"));
    assert(formatHex(0x9FC00, 8, buffer, sizeof(buffer)) == 10);
    assert(equal(buffer, "0x0009FC00"));
    // Padding is a minimum: large values are never truncated.
    assert(formatHex(0x123456789ULL, 4, buffer, sizeof(buffer)) == 11);
    assert(equal(buffer, "0x123456789"));
    assert(formatHex(UINT64_MAX, 1, buffer, sizeof(buffer)) == 18);
    assert(equal(buffer, "0xFFFFFFFFFFFFFFFF"));
    assert(formatHex(0xABC, 99, buffer, sizeof(buffer)) == 18);
    assert(equal(buffer, "0x0000000000000ABC"));

    // Hex: buffer boundaries.
    char hexSmall[11];
    assert(formatHex(0x1F, 8, hexSmall, sizeof(hexSmall)) == 10);
    assert(equal(hexSmall, "0x0000001F"));
    char hexTooSmall[10];
    assert(formatHex(0x1F, 8, hexTooSmall, sizeof(hexTooSmall)) == 0);
    assert(hexTooSmall[0] == '\0');
    assert(formatHex(1, 1, nullptr, 8) == 0);

    // Size.
    assert(formatSize(0, buffer, sizeof(buffer)) == 7);
    assert(equal(buffer, "0 bytes"));
    assert(formatSize(1023, buffer, sizeof(buffer)) == 10);
    assert(equal(buffer, "1023 bytes"));
    assert(formatSize(1024, buffer, sizeof(buffer)) == 5);
    assert(equal(buffer, "1 KiB"));
    assert(formatSize(1536, buffer, sizeof(buffer)) == 5);
    assert(equal(buffer, "1 KiB"));                // truncates
    assert(formatSize(MIB - 1, buffer, sizeof(buffer)) > 0);
    assert(equal(buffer, "1023 KiB"));
    assert(formatSize(MIB, buffer, sizeof(buffer)) == 5);
    assert(equal(buffer, "1 MiB"));
    assert(formatSize(0x7FE0000ULL, buffer, sizeof(buffer)) > 0);
    assert(equal(buffer, "127 MiB"));              // 127.875 truncated
    assert(formatSize(GIB, buffer, sizeof(buffer)) == 5);
    assert(equal(buffer, "1 GiB"));
    assert(formatSize(UINT64_MAX, buffer, sizeof(buffer)) > 0);
    assert(equal(buffer, "17179869183 GiB"));

    // Size: too-small buffers fail cleanly.
    char sizeSmall[7];
    assert(formatSize(1024, sizeSmall, sizeof(sizeSmall)) == 5);
    assert(equal(sizeSmall, "1 KiB"));
    char sizeTooSmall[5];
    assert(formatSize(1024, sizeTooSmall, sizeof(sizeTooSmall)) == 0);
    assert(sizeTooSmall[0] == '\0');
    assert(formatSize(1024, nullptr, 16) == 0);

    std::puts("Memory format tests passed.");
    return 0;
}
