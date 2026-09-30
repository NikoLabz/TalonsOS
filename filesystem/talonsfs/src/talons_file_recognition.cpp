#include "../include/talons_file_recognition.h"

#include "../include/talons_file_integrity.h"

namespace TalonsFile
{
    bool isTalonsFile(
        const uint8_t* data,
        uint64_t size)
    {
        return validateFile(data, size);
    }
}
