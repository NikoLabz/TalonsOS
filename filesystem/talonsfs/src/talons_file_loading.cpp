#include "../include/talons_file_loading.h"

#include "../include/talons_file_integrity.h"
#include "../include/talons_file_parsing.h"
#include "../include/talons_file_validator.h"

namespace TalonsFile
{
    bool loadPayload(
        const uint8_t* data,
        uint64_t size,
        uint8_t* output,
        uint64_t outputCapacity,
        uint64_t& outputSize)
    {
        outputSize = 0;

        if (data == nullptr)
        {
            return false;
        }

        if (output == nullptr)
        {
            return false;
        }

        if (!validateFile(data, size))
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

        if (header.payloadSize >
            outputCapacity)
        {
            return false;
        }

        const uint8_t* payload =
            data + header.headerSize;

        for (uint64_t i = 0;
             i < header.payloadSize;
             ++i)
        {
            output[i] = payload[i];
        }

        outputSize = header.payloadSize;

        return true;
    }
}
