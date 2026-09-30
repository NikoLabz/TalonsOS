#pragma once

#include <cstdint>

#include "talons_file.h"

namespace TalonsFile
{
    bool parseHeader(
        const uint8_t* input,
        uint32_t inputSize,
        Header& header
    );
}
