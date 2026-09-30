#include "../include/talons_file_serialization.h"

namespace TalonsFile
{
    bool serializeHeader(
        const Header& header,
        uint8_t* output,
        uint32_t outputSize)
    {
        if (output == nullptr ||
            outputSize < HEADER_SIZE)
        {
            return false;
        }

        output[0] = header.magic[0];
        output[1] = header.magic[1];
        output[2] = header.magic[2];
        output[3] = header.magic[3];

        output[4] = header.version;

        output[5] =
            static_cast<uint8_t>(
                header.headerSize & 0xFF
            );

        output[6] =
            static_cast<uint8_t>(
                (header.headerSize >> 8) & 0xFF
            );

        output[7] = header.fileType;

        for (unsigned int i = 0; i < 8; ++i)
        {
            output[8 + i] =
                static_cast<uint8_t>(
                    header.payloadSize >> (i * 8)
                );
        }

        for (unsigned int i = 0; i < 4; ++i)
        {
            output[16 + i] =
                static_cast<uint8_t>(
                    header.flags >> (i * 8)
                );

            output[20 + i] =
                static_cast<uint8_t>(
                    header.checksum >> (i * 8)
                );
        }

        return true;
    }
}
