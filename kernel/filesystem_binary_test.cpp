#include "filesystem.h"

bool test_filesystem_binary()
{
    const uint8_t binaryData[] = {
        0x00,
        0xFF,
        0x13,
        0xA7,
        0x00,
        0x42
    };

    if (!filesystem_write_file(
            "binary.tal",
            binaryData,
            sizeof(binaryData)))
    {
        return false;
    }

    uint8_t output[sizeof(binaryData)] = {};

    uint32_t outputSize = 0;

    if (!filesystem_read_file(
            "binary.tal",
            output,
            sizeof(output),
            outputSize))
    {
        return false;
    }

    if (outputSize != sizeof(binaryData))
    {
        return false;
    }

    for (uint32_t i = 0;
         i < sizeof(binaryData);
         ++i)
    {
        if (output[i] != binaryData[i])
        {
            return false;
        }
    }

    return true;
}
