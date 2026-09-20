#pragma once

#include <cstdint>

namespace TalonsFS
{
    constexpr uint64_t TALONSFS_MAGIC =
        0x54414C4F4E534653ULL;

    constexpr uint32_t TALONSFS_VERSION = 1;

    constexpr uint32_t TALONSFS_BLOCK_SIZE = 512;
}
