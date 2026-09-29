#include "talons_file_validator.h"

#include <cassert>
#include <cstdint>
#include <cstring>

int main()
{
    TalonsFile::Header header{};

    std::memcpy(
        header.magic,
        TalonsFile::MAGIC,
        sizeof(TalonsFile::MAGIC)
    );

    header.version = TalonsFile::VERSION;
    header.headerSize = TalonsFile::HEADER_SIZE;
    header.fileType = TalonsFile::TYPE_TEXT;
    header.payloadSize = 123;
    header.flags = 0;

    assert(
        TalonsFile::validateHeader(&header) ==
        TalonsFile::ValidationError::None
    );

    header.magic[0] = 'X';

    assert(
        TalonsFile::validateHeader(&header) ==
        TalonsFile::ValidationError::InvalidMagic
    );

    header.magic[0] = 'T';

    header.version = 0;

    assert(
        TalonsFile::validateHeader(&header) ==
        TalonsFile::ValidationError::UnsupportedVersion
    );

    header.version = TalonsFile::VERSION;

    header.headerSize = 23;

    assert(
        TalonsFile::validateHeader(&header) ==
        TalonsFile::ValidationError::InvalidHeaderSize
    );

    header.headerSize = TalonsFile::HEADER_SIZE;

    header.fileType = TalonsFile::TYPE_INVALID;

    assert(
        TalonsFile::validateHeader(&header) ==
        TalonsFile::ValidationError::InvalidFileType
    );

    header.fileType = TalonsFile::TYPE_TEXT;

    header.flags = TalonsFile::FLAG_RESERVED_MASK;

    assert(
        TalonsFile::validateHeader(&header) ==
        TalonsFile::ValidationError::InvalidFlags
    );

    header.flags = 0;

    assert(
        TalonsFile::validateHeader(nullptr) ==
        TalonsFile::ValidationError::NullHeader
    );

    return 0;
}
