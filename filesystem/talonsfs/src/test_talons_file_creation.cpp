#include <cassert>
#include <cstdint>

#include "../include/talons_file.h"
#include "../include/talons_file_creation.h"
#include "../include/talons_file_integrity.h"

int main()
{
    const uint8_t payload[] = {
        'H', 'e', 'l', 'l', 'o'
    };

    uint8_t output[64]{};
    uint64_t outputSize = 0;

    bool result = TalonsFile::createFile(
        TalonsFile::TYPE_TEXT,
        0,
        payload,
        sizeof(payload),
        output,
        sizeof(output),
        outputSize
    );

    assert(result);
    assert(outputSize ==
        TalonsFile::HEADER_SIZE + sizeof(payload));

    assert(output[0] == 'T');
    assert(output[1] == 'A');
    assert(output[2] == 'L');
    assert(output[3] == '0');

    assert(output[4] == TalonsFile::VERSION);
    assert(output[7] == TalonsFile::TYPE_TEXT);

    assert(TalonsFile::validateFile(
        output,
        outputSize
    ));

    for (uint64_t i = 0;
         i < sizeof(payload);
         ++i)
    {
        assert(
            output[TalonsFile::HEADER_SIZE + i]
            == payload[i]
        );
    }

        // Output buffer too small
    uint8_t smallOutput[10]{};
    outputSize = 123;

    assert(!TalonsFile::createFile(
        TalonsFile::TYPE_TEXT,
        0,
        payload,
        sizeof(payload),
        smallOutput,
        sizeof(smallOutput),
        outputSize
    ));

    assert(outputSize == 0);

    // Payload is null but size is non-zero
    outputSize = 123;

    assert(!TalonsFile::createFile(
        TalonsFile::TYPE_TEXT,
        0,
        nullptr,
        sizeof(payload),
        output,
        sizeof(output),
        outputSize
    ));

    assert(outputSize == 0);

    // Null output buffer
    outputSize = 123;

    assert(!TalonsFile::createFile(
        TalonsFile::TYPE_TEXT,
        0,
        payload,
        sizeof(payload),
        nullptr,
        sizeof(output),
        outputSize
    ));

    assert(outputSize == 0);

    // Empty payload is valid
    outputSize = 0;

    assert(TalonsFile::createFile(
        TalonsFile::TYPE_TEXT,
        0,
        nullptr,
        0,
        output,
        sizeof(output),
        outputSize
    ));

    assert(outputSize ==
        TalonsFile::HEADER_SIZE);

    assert(TalonsFile::validateFile(
        output,
        outputSize
    ));

    return 0;
}
