#pragma once

#include <cstdint>

#include "bitmap.h"
#include "block_allocator.h"
#include "node.h"
#include "node_table.h"

namespace TalonsFS {

class NodeDataManager {
public:
    NodeDataManager(
        BlockAllocator& blockAllocator,
        NodeTable& nodeTable
    );

    bool allocateBlock(uint64_t nodeIndex);

    bool freeBlocks(uint64_t nodeIndex);

private:
    BlockAllocator& blockAllocator_;
    NodeTable& nodeTable_;
};

}
