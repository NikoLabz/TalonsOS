#pragma once

#include "bitmap.h"
#include "block_device.h"
#include "node_table.h"

namespace TalonsFS {

bool validateNodeConsistency(
    const Bitmap& nodeBitmap,
    const NodeTable& nodeTable,
    uint64_t dataStart,
    uint64_t totalBlocks
);

bool validateBlockConsistency(
    const Bitmap& blockBitmap,
    const Bitmap& nodeBitmap,
    const NodeTable& nodeTable,
    uint64_t dataStart,
    uint64_t totalBlocks
);

bool validateDirectoryConsistency(
    BlockDevice& device,
    const Bitmap& nodeBitmap,
    const NodeTable& nodeTable
);

bool validateNodeReachability(
    BlockDevice& device,
    const Bitmap& nodeBitmap,
    const NodeTable& nodeTable
);

bool validateBitmapConsistency(
    const Bitmap& blockBitmap,
    const Bitmap& nodeBitmap,
    uint64_t dataStart,
    uint64_t totalBlocks,
    uint64_t expectedFreeBlocks,
    uint64_t expectedFreeNodes
);

bool validateMetadataConsistency(
    BlockDevice& device,
    const Bitmap& blockBitmap,
    const Bitmap& nodeBitmap,
    const NodeTable& nodeTable,
    uint64_t dataStart,
    uint64_t totalBlocks,
    uint64_t expectedFreeBlocks,
    uint64_t expectedFreeNodes
);

}
