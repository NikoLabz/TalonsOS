#include "talons_file_parsing.h"

#include <cassert>
#include <cstdint>
#include <cstring>

int main()
{
    const uint8_t input[24] = {
        'T', 'A', 'L', '0',
        1,
        24, 0,
        TalonsFile::TYPE_TEXT,

        0x08, 0x07, 0x06, 0x05,
        0x04, 0x03, 0x02, 0x01,

        0x01, 0x00, 0x00, 0x00,

        0xDD, 0xCC, 0xBB, 0xAA
    };

    TalonsFile::Header header{};

    assert(TalonsFile::parseHeader(
        input,
        sizeof(input),
        header
    ));

    assert(std::memcmp(
        header.magic,
        "TAL0",
        4
    ) == 0);

    assert(header.version == 1);
    assert(header.headerSize == 24);
    assert(header.fileType == TalonsFile::TYPE_TEXT);

    assert(
        header.payloadSize ==
        0x0102030405060708ULL
    );

    assert(header.flags == 1);
    assert(header.checksum == 0xAABBCCDD);

    assert(!TalonsFile::parseHeader(
        input,
        23,
        header
    ));

    assert(!TalonsFile::parseHeader(
        nullptr,
        24,
        header
    ));

    return 0;
}
