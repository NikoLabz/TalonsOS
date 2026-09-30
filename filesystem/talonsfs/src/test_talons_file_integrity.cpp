#include "talons_file_integrity.h"

#include "talons_file.h"
#include "talons_file_checksum.h"
#include "talons_file_serialization.h"

#include <cassert>
#include <cstdint>
#include <cstring>

int main()
{
    const uint8_t payload[] = {
        'H', 'e', 'l', 'l', 'o'
    };

    TalonsFile::Header header{};

    std::memcpy(
        header.magic,
        TalonsFile::MAGIC,
        sizeof(TalonsFile::MAGIC)
    );

    header.version = TalonsFile::VERSION;
    header.headerSize = TalonsFile::HEADER_SIZE;
    header.fileType = TalonsFile::TYPE_TEXT;
    header.payloadSize = sizeof(payload);
    header.flags = 0;
    header.checksum = 0;

    uint8_t file[
        TalonsFile::HEADER_SIZE + sizeof(payload)
    ]{};

    assert(TalonsFile::serializeHeader(
        header,
        file,
        TalonsFile::HEADER_SIZE
    ));

    std::memcpy(
        file + TalonsFile::HEADER_SIZE,
        payload,
        sizeof(payload)
    );

    uint8_t checksumData[
    20 + sizeof(payload)
]{};

std::memcpy(
    checksumData,
    file,
    20
);

std::memcpy(
    checksumData + 20,
    payload,
    sizeof(payload)
);

header.checksum =
    talons_file_crc32c(
        checksumData,
        sizeof(checksumData)
    );

    assert(TalonsFile::serializeHeader(
        header,
        file,
        TalonsFile::HEADER_SIZE
    ));

    std::memcpy(
        file + TalonsFile::HEADER_SIZE,
        payload,
        sizeof(payload)
    );

    assert(TalonsFile::validateFile(
        file,
        sizeof(file)
    ));

    file[TalonsFile::HEADER_SIZE] ^= 0xFF;

    assert(!TalonsFile::validateFile(
        file,
        sizeof(file)
    ));

    return 0;
}
