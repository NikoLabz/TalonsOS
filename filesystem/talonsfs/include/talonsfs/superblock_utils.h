#pragma once

#include "block_device.h"
#include "superblock.h"

namespace TalonsFS {

uint64_t calculateSuperblockChecksum(
    const Superblock& superblock
);

bool validateSuperblock(
    const Superblock& superblock
);

bool readSuperblock(
    BlockDevice& device,
    Superblock& superblock
);

}
