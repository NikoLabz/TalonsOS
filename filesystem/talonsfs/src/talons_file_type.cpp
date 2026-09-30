#include "../include/talons_file_type.h"

#include "../include/talons_file_integrity.h"
#include "../include/talons_file_parsing.h"

namespace TalonsFile
{
    bool getFileType(
        const uint8_t* data,
        uint64_t size,
        uint8_t& fileType)
    {
        fileType = TYPE_INVALID;

        if (data == nullptr)
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

        fileType = header.fileType;

        return true;
    }
}
