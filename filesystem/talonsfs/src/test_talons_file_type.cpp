#include <cassert>
#include <cstdint>

#include "../include/talons_file.h"
#include "../include/talons_file_creation.h"
#include "../include/talons_file_type.h"

int main()
{
    const uint8_t payload[] = {
        'T', 'a', 'l', 'o', 'n', 's'
    };

    uint8_t talFile[128]{};
    uint64_t talFileSize = 0;
    uint8_t fileType = 255;

    assert(TalonsFile::createFile(
        TalonsFile::TYPE_TEXT,
        0,
        payload,
        sizeof(payload),
        talFile,
        sizeof(talFile),
        talFileSize
    ));

    assert(TalonsFile::getFileType(
        talFile,
        talFileSize,
        fileType
    ));

    assert(fileType == TalonsFile::TYPE_TEXT);

    assert(TalonsFile::createFile(
        TalonsFile::TYPE_EXECUTABLE,
        0,
        payload,
        sizeof(payload),
        talFile,
        sizeof(talFile),
        talFileSize
    ));

    fileType = 255;

    assert(TalonsFile::getFileType(
        talFile,
        talFileSize,
        fileType
    ));

    assert(fileType == TalonsFile::TYPE_EXECUTABLE);

    assert(TalonsFile::createFile(
        TalonsFile::TYPE_BINARY,
        0,
        payload,
        sizeof(payload),
        talFile,
        sizeof(talFile),
        talFileSize
    ));

    fileType = 255;

    assert(TalonsFile::getFileType(
        talFile,
        talFileSize,
        fileType
    ));

    assert(fileType == TalonsFile::TYPE_BINARY);

    fileType = 255;

    assert(!TalonsFile::getFileType(
        nullptr,
        talFileSize,
        fileType
    ));

    assert(fileType == TalonsFile::TYPE_INVALID);

    fileType = 255;

    assert(!TalonsFile::getFileType(
        talFile,
        talFileSize - 1,
        fileType
    ));

    assert(fileType == TalonsFile::TYPE_INVALID);

    uint8_t corrupted[128]{};

    for (uint64_t i = 0;
         i < talFileSize;
         ++i)
    {
        corrupted[i] = talFile[i];
    }

    corrupted[TalonsFile::HEADER_SIZE] ^= 0xFF;

    fileType = 255;

    assert(!TalonsFile::getFileType(
        corrupted,
        talFileSize,
        fileType
    ));

    assert(fileType == TalonsFile::TYPE_INVALID);

    return 0;
}
