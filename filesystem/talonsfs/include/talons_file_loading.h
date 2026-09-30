#pragma once

#include <cstdint>

namespace TalonsFile
{
    bool loadPayload(
        const uint8_t* data,
        uint64_t size,
        uint8_t* output,
        uint64_t outputCapacity,
        uint64_t& outputSize
    );
}
