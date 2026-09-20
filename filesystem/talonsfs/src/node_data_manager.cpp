#include "../include/talonsfs/node_data_manager.h"

namespace TalonsFS {

NodeDataManager::NodeDataManager(
    BlockAllocator& blockAllocator,
    NodeTable& nodeTable
)
    : blockAllocator_(blockAllocator),
      nodeTable_(nodeTable)
{
}

bool NodeDataManager::allocateBlock(uint64_t nodeIndex)
{
    Node node = nodeTable_.read(nodeIndex);

    if (node.type == NodeType::Free) {
        return false;
    }

    if (node.dataBlocks == 0) {
        const int64_t block =
            blockAllocator_.allocate();

        if (block < 0) {
            return false;
        }

        node.dataStart =
            static_cast<uint64_t>(block);

        node.dataBlocks = 1;
    }
    else
    {
        const uint64_t nextBlock =
            node.dataStart + node.dataBlocks;

        if (!blockAllocator_.allocateSpecific(nextBlock)) {
            return false;
        }

        ++node.dataBlocks;
    }

    nodeTable_.write(
        nodeIndex,
        node
    );

    return true;
}

bool NodeDataManager::freeBlocks(uint64_t nodeIndex)
{
    Node node = nodeTable_.read(nodeIndex);

    if (node.type == NodeType::Free) {
        return false;
    }

    for (uint64_t i = 0; i < node.dataBlocks; ++i) {
        blockAllocator_.free(
            node.dataStart + i
        );
    }

    node.dataStart = 0;
    node.dataBlocks = 0;

    nodeTable_.write(
        nodeIndex,
        node
    );

    return true;
}

}
