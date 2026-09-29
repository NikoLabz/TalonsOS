#pragma once

#include <cstdint>

#include "talons_file.h"

namespace TalonsFile
{
    enum class ValidationError
    {
        None,
        NullHeader,
        InvalidMagic,
        UnsupportedVersion,
        InvalidHeaderSize,
        InvalidFileType,
        InvalidPayloadSize,
        InvalidFlags,
        InvalidChecksum
    };

    ValidationError validateHeader(
        const Header* header
    );
}
