#include <iostream>
#include <cstdint>

#include "../include/talonsfs/bitmap.h"
#include "../include/talonsfs/block_allocator.h"
#include "../include/talonsfs/node_table.h"
#include "../include/talonsfs/node_data_manager.h"

int main()
{
    constexpr uint64_t nodeCount = 4;
    constexpr uint64_t totalBlocks = 32;
    constexpr uint64_t dataStart = 20;

    uint8_t bitmapData[4] = {};

    TalonsFS::Bitmap bitmap(
        bitmapData,
        totalBlocks
    );

    TalonsFS::BlockAllocator blockAllocator(
        bitmap,
        totalBlocks,
        dataStart
    );

    uint8_t nodeData[
        nodeCount * sizeof(TalonsFS::Node)
    ] = {};

    TalonsFS::NodeTable nodeTable(
        nodeData,
        nodeCount
    );

    TalonsFS::Node file{};

    file.type = TalonsFS::NodeType::File;

    nodeTable.write(1, file);

    TalonsFS::NodeDataManager manager(
        blockAllocator,
        nodeTable
    );

    bool allocated =
        manager.allocateBlock(1);

    TalonsFS::Node loaded =
        nodeTable.read(1);

    std::cout << "Block allocated: "
              << allocated
              << '\n';

    std::cout << "Data start: "
              << loaded.dataStart
              << '\n';

    std::cout << "Data blocks: "
              << loaded.dataBlocks
              << '\n';

    manager.freeBlocks(1);

    loaded = nodeTable.read(1);

    std::cout << "After free, data start: "
              << loaded.dataStart
              << '\n';

    std::cout << "After free, data blocks: "
              << loaded.dataBlocks
              << '\n';

    return 0;
}
