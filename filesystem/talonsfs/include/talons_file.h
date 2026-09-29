#pragma once

#include <cstdint>

namespace TalonsFile
{
    constexpr char MAGIC[4] = {
        'T', 'A', 'L', '0'
    };

    constexpr uint8_t VERSION = 1;
    constexpr uint16_t HEADER_SIZE = 24;
    constexpr uint32_t CHECKSUM_SIZE = 4;

    using PayloadSize = uint64_t;

    constexpr uint8_t TYPE_INVALID    = 0;
    constexpr uint8_t TYPE_TEXT       = 1;
    constexpr uint8_t TYPE_EXECUTABLE = 2;
    constexpr uint8_t TYPE_BINARY     = 3;
    constexpr uint8_t TYPE_CONFIG     = 4;
    constexpr uint8_t TYPE_LIBRARY    = 5;

    constexpr uint32_t FLAG_READ_ONLY = 1u << 0;
    constexpr uint32_t FLAG_RESERVED_MASK =
        0xFFFFFFFEu;

#pragma pack(push, 1)

    struct Header
    {
        char magic[4];
        uint8_t version;
        uint16_t headerSize;
        uint8_t fileType;
        uint64_t payloadSize;
        uint32_t flags;
        uint32_t checksum;
    };

#pragma pack(pop)

    static_assert(
        sizeof(Header) == HEADER_SIZE,
        "TalonsFile::Header must be exactly 24 bytes"
    );
}
