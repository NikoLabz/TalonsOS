#pragma once

#include <cstdint>

namespace TalonsFile
{
    bool validateFile(
        const uint8_t* data,
        uint64_t size
    );
}
