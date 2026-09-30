#pragma once

#include <cstdint>

namespace TalonsFile
{
    bool isTalonsFile(
        const uint8_t* data,
        uint64_t size
    );
}
