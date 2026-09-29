#include "talons_file_checksum.h"

#include <cassert>
#include <cstdint>

int main()
{
    const uint8_t data[] = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };

    const uint32_t checksum =
        talons_file_crc32c(
            data,
            sizeof(data)
        );

    assert(checksum == 0xE3069283u);

    return 0;
}
