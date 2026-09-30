#include "../include/talons_file_parsing.h"

namespace TalonsFile
{
    bool parseHeader(
        const uint8_t* input,
        uint32_t inputSize,
        Header& header)
    {
        if (input == nullptr ||
            inputSize < HEADER_SIZE)
        {
            return false;
        }

        header.magic[0] = input[0];
        header.magic[1] = input[1];
        header.magic[2] = input[2];
        header.magic[3] = input[3];

        header.version = input[4];

        header.headerSize =
            static_cast<uint16_t>(
                input[5] |
                (static_cast<uint16_t>(input[6]) << 8)
            );

        header.fileType = input[7];

        header.payloadSize = 0;

        for (unsigned int i = 0; i < 8; ++i)
        {
            header.payloadSize |=
                static_cast<uint64_t>(input[8 + i])
                << (i * 8);
        }

        header.flags = 0;

        for (unsigned int i = 0; i < 4; ++i)
        {
            header.flags |=
                static_cast<uint32_t>(input[16 + i])
                << (i * 8);
        }

        header.checksum = 0;

        for (unsigned int i = 0; i < 4; ++i)
        {
            header.checksum |=
                static_cast<uint32_t>(input[20 + i])
                << (i * 8);
        }

        return true;
    }
}
