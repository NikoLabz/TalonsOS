#include "../include/talons_file_integrity.h"

#include "../include/talons_file.h"
#include "../include/talons_file_checksum.h"
#include "../include/talons_file_parsing.h"
#include "../include/talons_file_validator.h"

namespace TalonsFile
{
    bool validateFile(
        const uint8_t* data,
        uint64_t size)
    {
        if (data == nullptr ||
            size < HEADER_SIZE)
        {
            return false;
        }

        Header header{};

        if (!parseHeader(
                data,
                HEADER_SIZE,
                header))
        {
            return false;
        }

        if (validateHeader(&header) !=
            ValidationError::None)
        {
            return false;
        }

        if (header.headerSize > size)
        {
            return false;
        }

        const uint64_t totalSize =
            static_cast<uint64_t>(
                header.headerSize
            ) +
            header.payloadSize;

        if (totalSize > size)
        {
            return false;
        }

        uint32_t crc = 0xFFFFFFFFu;

        for (uint64_t i = 0;
             i < header.headerSize;
             ++i)
        {
            if (i >= 20 && i < 24)
            {
                continue;
            }

            crc ^= data[i];

            for (unsigned int bit = 0;
                 bit < 8;
                 ++bit)
            {
                if (crc & 1u)
                {
                    crc =
                        (crc >> 1) ^
                        0x82F63B78u;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }

        const uint8_t* payload =
            data + header.headerSize;

        for (uint64_t i = 0;
             i < header.payloadSize;
             ++i)
        {
            crc ^= payload[i];

            for (unsigned int bit = 0;
                 bit < 8;
                 ++bit)
            {
                if (crc & 1u)
                {
                    crc =
                        (crc >> 1) ^
                        0x82F63B78u;
                }
                else
                {
                    crc >>= 1;
                }
            }
        }

        crc = ~crc;

        return crc == header.checksum;
    }
}
