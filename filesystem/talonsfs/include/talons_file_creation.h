#pragma once

#include <cstdint>

namespace TalonsFile
{
    bool createFile(
        uint8_t fileType,
        uint32_t flags,
        const uint8_t* payload,
        uint64_t payloadSize,
        uint8_t* output,
        uint64_t outputCapacity,
        uint64_t& outputSize
    );
}
