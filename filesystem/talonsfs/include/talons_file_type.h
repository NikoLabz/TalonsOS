#pragma once

#include <cstdint>

namespace TalonsFile
{
    bool getFileType(
        const uint8_t* data,
        uint64_t size,
        uint8_t& fileType
    );
}
