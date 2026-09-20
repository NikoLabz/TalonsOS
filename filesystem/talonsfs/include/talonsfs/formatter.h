#pragma once

#include "block_device.h"
#include "superblock.h"

namespace TalonsFS {

Superblock createSuperblock(BlockDevice& device);

bool format(
    BlockDevice& device,
    Superblock& superblock
);

}
