#pragma once

#include <cstdint>

#include "block_device.h"
#include "data_block_manager.h"
#include "node.h"
#include "node_table.h"

namespace TalonsFS {

class FileDataManager {
public:
    FileDataManager(
        DataBlockManager& dataBlocks,
        NodeTable& nodeTable,
        uint64_t dataStart,
        uint64_t totalBlocks
    );

    bool write(
        uint64_t nodeIndex,
        const void* buffer,
        uint64_t size
    );

    bool read(
        uint64_t nodeIndex,
        void* buffer,
        uint64_t size
    ) const;

private:
    DataBlockManager& dataBlocks_;
    NodeTable& nodeTable_;
    uint64_t dataStart_;
    uint64_t totalBlocks_;
};

}
