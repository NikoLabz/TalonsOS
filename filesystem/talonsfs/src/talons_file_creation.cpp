#include "../include/talons_file_creation.h"

#include "../include/talons_file.h"
#include "../include/talons_file_checksum.h"
#include "../include/talons_file_serialization.h"
#include "../include/talons_file_validator.h"

namespace TalonsFile
{
    bool createFile(
        uint8_t fileType,
        uint32_t flags,
        const uint8_t* payload,
        uint64_t payloadSize,
        uint8_t* output,
        uint64_t outputCapacity,
        uint64_t& outputSize)
    {
        outputSize = 0;

        if (output == nullptr)
        {
            return false;
        }

        if (payloadSize > 0 &&
            payload == nullptr)
        {
            return false;
        }

        if (payloadSize >
            UINT64_MAX - HEADER_SIZE)
        {
            return false;
        }

        const uint64_t totalSize =
            static_cast<uint64_t>(HEADER_SIZE) +
            payloadSize;

        if (outputCapacity < totalSize)
        {
            return false;
        }

        Header header{};

        header.magic[0] = MAGIC[0];
        header.magic[1] = MAGIC[1];
        header.magic[2] = MAGIC[2];
        header.magic[3] = MAGIC[3];

        header.version = VERSION;
        header.headerSize = HEADER_SIZE;
        header.fileType = fileType;
        header.payloadSize = payloadSize;
        header.flags = flags;
        header.checksum = 0;

        if (validateHeader(&header) !=
            ValidationError::None)
        {
            return false;
        }

        if (!serializeHeader(
                header,
                output,
                HEADER_SIZE))
        {
            return false;
        }

        if (payloadSize > 0)
        {
            for (uint64_t i = 0;
                 i < payloadSize;
                 ++i)
            {
                output[HEADER_SIZE + i] =
                    payload[i];
            }
        }

        uint32_t crc = talons_file_crc32c(
            output,
            20
        );

        if (payloadSize > 0)
        {
            uint32_t payloadCrc =
                talons_file_crc32c(
                    payload,
                    payloadSize
                );

            // Combine the CRC streams by
            // calculating the checksum over
            // the actual serialized data.
            //
            // The simple two-call result above
            // cannot be directly XORed, so
            // calculate the complete stream below.
            (void)crc;
            (void)payloadCrc;
        }

        uint32_t checksum = 0xFFFFFFFFu;

        for (uint64_t i = 0;
             i < 20;
             ++i)
        {
            checksum ^= output[i];

            for (unsigned int bit = 0;
                 bit < 8;
                 ++bit)
            {
                if (checksum & 1u)
                {
                    checksum =
                        (checksum >> 1) ^
                        0x82F63B78u;
                }
                else
                {
                    checksum >>= 1;
                }
            }
        }

        for (uint64_t i = 0;
             i < payloadSize;
             ++i)
        {
            checksum ^= output[
                HEADER_SIZE + i
            ];

            for (unsigned int bit = 0;
                 bit < 8;
                 ++bit)
            {
                if (checksum & 1u)
                {
                    checksum =
                        (checksum >> 1) ^
                        0x82F63B78u;
                }
                else
                {
                    checksum >>= 1;
                }
            }
        }

        checksum = ~checksum;

        output[20] =
            static_cast<uint8_t>(
                checksum & 0xFF
            );

        output[21] =
            static_cast<uint8_t>(
                (checksum >> 8) & 0xFF
            );

        output[22] =
            static_cast<uint8_t>(
                (checksum >> 16) & 0xFF
            );

        output[23] =
            static_cast<uint8_t>(
                (checksum >> 24) & 0xFF
            );

        outputSize = totalSize;

        return true;
    }
}
