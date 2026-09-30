#include <cassert>
#include <cstdint>

#include "../include/talons_file.h"
#include "../include/talons_file_creation.h"
#include "../include/talons_file_recognition.h"

int main()
{
    const uint8_t payload[] = {
        'T', 'a', 'l', 'o', 'n', 's'
    };

    uint8_t output[64]{};
    uint64_t outputSize = 0;

    // Valid .tal file
    assert(TalonsFile::createFile(
        TalonsFile::TYPE_TEXT,
        0,
        payload,
        sizeof(payload),
        output,
        sizeof(output),
        outputSize
    ));

    assert(TalonsFile::isTalonsFile(
        output,
        outputSize
    ));

    // Invalid magic
    uint8_t badMagic[64]{};

    for (uint64_t i = 0; i < outputSize; ++i)
    {
        badMagic[i] = output[i];
    }

    badMagic[0] = 'X';

    assert(!TalonsFile::isTalonsFile(
        badMagic,
        outputSize
    ));

    // Corrupted payload
    uint8_t corrupted[64]{};

    for (uint64_t i = 0; i < outputSize; ++i)
    {
        corrupted[i] = output[i];
    }

    corrupted[
        TalonsFile::HEADER_SIZE
    ] ^= 0xFF;

    assert(!TalonsFile::isTalonsFile(
        corrupted,
        outputSize
    ));

    // Truncated file
    assert(!TalonsFile::isTalonsFile(
        output,
        outputSize - 1
    ));

    // Null data
    assert(!TalonsFile::isTalonsFile(
        nullptr,
        outputSize
    ));

    // Empty input
    assert(!TalonsFile::isTalonsFile(
        output,
        0
    ));

    return 0;
}
