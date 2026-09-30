#include <cassert>
#include <cstdint>

#include "../include/talons_file.h"
#include "../include/talons_file_creation.h"
#include "../include/talons_file_loading.h"

int main()
{
    const uint8_t payload[] = {
        'H', 'e', 'l', 'l', 'o',
        ' ', 'T', 'a', 'l', 'o', 'n', 's', 'O', 'S'
    };

    uint8_t talFile[128]{};
    uint64_t talFileSize = 0;

    // Create a valid .tal file.
    assert(TalonsFile::createFile(
        TalonsFile::TYPE_TEXT,
        0,
        payload,
        sizeof(payload),
        talFile,
        sizeof(talFile),
        talFileSize
    ));

    // Load the payload.
    uint8_t output[128]{};
    uint64_t outputSize = 999;

    assert(TalonsFile::loadPayload(
        talFile,
        talFileSize,
        output,
        sizeof(output),
        outputSize
    ));

    assert(outputSize == sizeof(payload));

    for (uint64_t i = 0;
         i < sizeof(payload);
         ++i)
    {
        assert(output[i] == payload[i]);
    }

    // Output buffer too small.
    uint8_t smallOutput[4]{};
    outputSize = 999;

    assert(!TalonsFile::loadPayload(
        talFile,
        talFileSize,
        smallOutput,
        sizeof(smallOutput),
        outputSize
    ));

    assert(outputSize == 0);

    // Null input.
    outputSize = 999;

    assert(!TalonsFile::loadPayload(
        nullptr,
        talFileSize,
        output,
        sizeof(output),
        outputSize
    ));

    assert(outputSize == 0);

    // Null output.
    outputSize = 999;

    assert(!TalonsFile::loadPayload(
        talFile,
        talFileSize,
        nullptr,
        sizeof(output),
        outputSize
    ));

    assert(outputSize == 0);

    // Corrupt the payload. Loading must reject it.
    uint8_t corrupted[128]{};

    for (uint64_t i = 0;
         i < talFileSize;
         ++i)
    {
        corrupted[i] = talFile[i];
    }

    corrupted[TalonsFile::HEADER_SIZE] ^= 0xFF;

    outputSize = 999;

    assert(!TalonsFile::loadPayload(
        corrupted,
        talFileSize,
        output,
        sizeof(output),
        outputSize
    ));

    assert(outputSize == 0);

    // Truncated .tal file.
    outputSize = 999;

    assert(!TalonsFile::loadPayload(
        talFile,
        talFileSize - 1,
        output,
        sizeof(output),
        outputSize
    ));

    assert(outputSize == 0);

    // Empty payload is valid.
    uint8_t emptyTalFile[64]{};
    uint64_t emptyTalFileSize = 0;

    assert(TalonsFile::createFile(
        TalonsFile::TYPE_TEXT,
        0,
        nullptr,
        0,
        emptyTalFile,
        sizeof(emptyTalFile),
        emptyTalFileSize
    ));

    outputSize = 999;

    assert(TalonsFile::loadPayload(
        emptyTalFile,
        emptyTalFileSize,
        output,
        sizeof(output),
        outputSize
    ));

    assert(outputSize == 0);

    return 0;
}
