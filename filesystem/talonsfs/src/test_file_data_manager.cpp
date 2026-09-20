#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "../include/talonsfs/memory_block_device.h"
#include "../include/talonsfs/data_block_manager.h"
#include "../include/talonsfs/node_table.h"
#include "../include/talonsfs/node_data_manager.h"
#include "../include/talonsfs/file_data_manager.h"
#include "../include/talonsfs/bitmap.h"
#include "../include/talonsfs/block_allocator.h"

int main()
{
    constexpr uint64_t nodeCount = 4;
    constexpr uint64_t totalBlocks = 32;
    constexpr uint64_t dataStart = 20;
    constexpr uint32_t blockSize = 4096;

    TalonsFS::MemoryBlockDevice device(
        totalBlocks,
        blockSize
    );

    TalonsFS::DataBlockManager dataBlocks(device);

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

    TalonsFS::NodeDataManager nodeDataManager(
        blockAllocator,
        nodeTable
    );

    nodeDataManager.allocateBlock(1);
    nodeDataManager.allocateBlock(1);

    TalonsFS::FileDataManager fileData(
        dataBlocks,
        nodeTable,
        dataStart,
        totalBlocks
    );

    const uint64_t size = blockSize + 100;

    std::vector<uint8_t> writeBuffer(size);
    std::vector<uint8_t> readBuffer(size);

    for (uint64_t i = 0; i < size; ++i) {
        writeBuffer[i] =
            static_cast<uint8_t>(i % 256);
    }

    bool writeSuccess =
        fileData.write(
            1,
            writeBuffer.data(),
            size
        );

    bool readSuccess =
        fileData.read(
            1,
            readBuffer.data(),
            size
        );

    bool identical =
        writeBuffer == readBuffer;

    TalonsFS::Node loaded =
        nodeTable.read(1);

    std::cout << "Write successful: "
              << writeSuccess
              << '\n';

    std::cout << "Read successful: "
              << readSuccess
              << '\n';

    std::cout << "Data identical: "
              << identical
              << '\n';

    std::cout << "File size: "
              << loaded.size
              << '\n';

    std::cout << "Data blocks: "
              << loaded.dataBlocks
              << '\n';

    return 0;
}
