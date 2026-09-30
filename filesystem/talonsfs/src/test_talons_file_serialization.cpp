#include "talons_file_serialization.h"

#include <cassert>
#include <cstdint>
#include <cstring>

int main()
{
    TalonsFile::Header header{};

    std::memcpy(header.magic, "TAL0", 4);
    header.version = 1;
    header.headerSize = 24;
    header.fileType = TalonsFile::TYPE_TEXT;
    header.payloadSize = 0x0102030405060708ULL;
    header.flags = 1;
    header.checksum = 0xAABBCCDD;

    uint8_t output[24]{};

    assert(TalonsFile::serializeHeader(
        header, output, sizeof(output)));

    assert(std::memcmp(output, "TAL0", 4) == 0);
    assert(output[4] == 1);
    assert(output[5] == 24);
    assert(output[6] == 0);
    assert(output[7] == TalonsFile::TYPE_TEXT);

    assert(output[8] == 0x08);
    assert(output[9] == 0x07);
    assert(output[15] == 0x01);

    assert(output[16] == 1);
    assert(output[20] == 0xDD);
    assert(output[21] == 0xCC);
    assert(output[22] == 0xBB);
    assert(output[23] == 0xAA);

    assert(!TalonsFile::serializeHeader(
        header, output, 23));

    assert(!TalonsFile::serializeHeader(
        header, nullptr, 24));

    return 0;
}
