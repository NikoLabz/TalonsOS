#pragma once

#include <cstdint>

#include "talons_file.h"

namespace TalonsFile
{
    bool serializeHeader(
        const Header& header,
        uint8_t* output,
        uint32_t outputSize
    );
}
