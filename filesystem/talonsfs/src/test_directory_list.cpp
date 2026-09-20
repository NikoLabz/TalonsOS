#include <iostream>
#include <cstring>
#include <vector>

#include "../include/talonsfs/memory_block_device.h"
#include "../include/talonsfs/formatter.h"
#include "../include/talonsfs/directory.h"
#include "../include/talonsfs/directory_entry.h"
#include "../include/talonsfs/node_table.h"
#include "../include/talonsfs/node_data_manager.h"
#include "../include/talonsfs/data_block_manager.h"
#include "../include/talonsfs/bitmap.h"
#include "../include/talonsfs/block_allocator.h"

int main()
{
    constexpr uint64_t totalBlocks = 4096;
    constexpr uint32_t blockSize = 4096;

    TalonsFS::MemoryBlockDevice device(
        totalBlocks,
        blockSize
    );


    TalonsFS::Superblock superblock{};

    bool formatted =
        TalonsFS::format(
            device,
            superblock
        );


    std::cout
        << "Formatted: "
        << formatted
        << '\n';


    TalonsFS::DataBlockManager dataBlocks(
        device
    );


    std::vector<uint8_t> bitmapMemory(
        superblock.totalBlocks / 8 + 1
    );


    TalonsFS::Bitmap blockBitmap(
        bitmapMemory.data(),
        superblock.totalBlocks
    );


    TalonsFS::BlockAllocator allocator(
        blockBitmap,
	superblock.totalBlocks,
	superblock.dataStart
);

    std::vector<uint8_t> nodeMemory(
        superblock.nodeCount * sizeof(TalonsFS::Node)
    );


    TalonsFS::NodeTable nodeTable(
        nodeMemory.data(),
        superblock.nodeCount
    );

    TalonsFS::Node root{};

root.type = TalonsFS::NodeType::Directory;
root.size = 0;
root.dataStart = 0;
root.dataBlocks = 0;

nodeTable.write(
    0,
    root
);

    TalonsFS::NodeDataManager nodeData(
        allocator,
        nodeTable
    );


    TalonsFS::Directory directory(
        dataBlocks,
        nodeTable,
        nodeData
    );


    const char* names[] =
    {
        "Laboratory",
        "DoTheThings",
        "PleaseConfigureThese",
        "ThingsKeepChanging",
        "ProbablyTemporary",
        "HumanThings",
        "ThingsWithWires",
        "TheSystemIsBusy",
        "ExtraThings",
        "TheServerIsWorking",
        "PutThingsHere",
        "UnpluggedThings",
        "WeFoundThese"
    };


    for (const char* name : names)
    {
        TalonsFS::DirectoryEntry entry{};


        std::strncpy(
            entry.name,
            name,
            TalonsFS::MaxFilenameLength
        );


        entry.node = 1;
        entry.type = 2;


        bool added =
            directory.add(
                0,
                entry
            );


        std::cout
            << "Added "
            << name
            << ": "
            << added
            << '\n';
    }


    auto entries =
        directory.list(0);


    std::cout
        << "\nDirectory contents:\n";


    for (const auto& entry : entries)
    {
        std::cout
            << "[DIR] "
            << entry.name
            << '\n';
    }


    std::cout
        << "\nObjects: "
        << entries.size()
        << '\n';


    return 0;
}
