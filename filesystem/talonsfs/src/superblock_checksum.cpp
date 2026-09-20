#include <cstddef>
#include <cstdint>

#include "../include/talonsfs/superblock_utils.h"

namespace TalonsFS {

uint64_t calculateSuperblockChecksum(
    const Superblock& superblock
)
{
    const uint8_t* bytes =
        reinterpret_cast<const uint8_t*>(&superblock);

    uint64_t checksum = 1469598103934665603ULL;

    for (uint64_t i = 0;
         i < sizeof(Superblock);
         ++i)
    {
        if (i >= offsetof(Superblock, checksum) &&
            i < offsetof(Superblock, checksum) +
                sizeof(superblock.checksum))
        {
            continue;
        }

        checksum ^= bytes[i];
        checksum *= 1099511628211ULL;
    }

    return checksum;
}

}
