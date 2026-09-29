#include "../include/talons_file_checksum.h"

uint32_t talons_file_crc32c(
    const uint8_t* data,
    uint64_t size)
{
    uint32_t crc = 0xFFFFFFFFu;

    for (uint64_t i = 0; i < size; ++i)
    {
        crc ^= data[i];

        for (unsigned int bit = 0;
             bit < 8;
             ++bit)
        {
            if (crc & 1u)
            {
                crc =
                    (crc >> 1) ^
                    0x82F63B78u;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}
