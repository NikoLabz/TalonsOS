#include <cstddef>
#include <cstring>

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

bool validateSuperblock(
    const Superblock& superblock
)
{
    if (superblock.magic !=
        0x54414C4F4E534653ULL)
    {
        return false;
    }

    if (superblock.version != 1)
    {
        return false;
    }

    if (superblock.blockSize == 0 ||
        superblock.totalBlocks == 0)
    {
        return false;
    }

    if (superblock.blockBitmapStart < 2)
{
    return false;
}

if (superblock.blockBitmapStart +
        superblock.blockBitmapBlocks >
    superblock.totalBlocks)
{
    return false;
}

if (superblock.nodeBitmapStart <
    superblock.blockBitmapStart +
        superblock.blockBitmapBlocks)
{
    return false;
}

if (superblock.nodeBitmapStart +
        superblock.nodeBitmapBlocks >
    superblock.totalBlocks)
{
    return false;
}

if (superblock.nodeTableStart <
    superblock.nodeBitmapStart +
        superblock.nodeBitmapBlocks)
{
    return false;
}

if (superblock.nodeTableStart +
        superblock.nodeTableBlocks >
    superblock.totalBlocks)
{
    return false;
}

if (superblock.dataStart <
    superblock.nodeTableStart +
        superblock.nodeTableBlocks)
{
    return false;
}

if (superblock.dataStart >=
    superblock.totalBlocks)
{
    return false;
}

if (superblock.rootNode >=
    superblock.nodeCount)
{
    return false;
}

return calculateSuperblockChecksum(
    superblock
) == superblock.checksum;
}

bool readSuperblock(
    BlockDevice& device,
    Superblock& superblock
)
{
    const uint32_t blockSize =
        device.getBlockSize();

    if (blockSize < sizeof(Superblock))
    {
        return false;
    }

    uint8_t* buffer =
        new uint8_t[blockSize];

    if (!device.readBlock(
            1,
            buffer))
    {
        delete[] buffer;
        return false;
    }

    std::memcpy(
        &superblock,
        buffer,
        sizeof(Superblock)
    );

    delete[] buffer;

    return validateSuperblock(
        superblock
    );
}

}
