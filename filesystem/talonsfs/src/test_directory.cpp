#include <iostream>
#include <cstring>
#include <cstdint>

#include "../include/talonsfs/memory_block_device.h"
#include "../include/talonsfs/bitmap.h"
#include "../include/talonsfs/block_allocator.h"
#include "../include/talonsfs/node_allocator.h"
#include "../include/talonsfs/node_table.h"
#include "../include/talonsfs/node_manager.h"
#include "../include/talonsfs/node_data_manager.h"
#include "../include/talonsfs/data_block_manager.h"
#include "../include/talonsfs/directory.h"
#include "../include/talonsfs/directory_entry.h"
#include "../include/talonsfs/layout.h"

int main()
{
    constexpr uint64_t totalBlocks = 32;
    constexpr uint64_t nodeCount = 4;
    constexpr uint64_t dataStart = 20;
    constexpr uint32_t blockSize = 4096;

    TalonsFS::MemoryBlockDevice device(
        totalBlocks,
        blockSize
    );

    uint8_t blockBitmapData[4] = {};

    TalonsFS::Bitmap blockBitmap(
        blockBitmapData,
        totalBlocks
    );

    TalonsFS::BlockAllocator blockAllocator(
        blockBitmap,
        totalBlocks,
        dataStart
    );

    uint8_t nodeBitmapData[1] = {};

    TalonsFS::Bitmap nodeBitmap(
        nodeBitmapData,
        nodeCount
    );

    uint8_t nodeData[
        nodeCount * sizeof(TalonsFS::Node)
    ] = {};

    TalonsFS::NodeTable nodeTable(
        nodeData,
        nodeCount
    );

    TalonsFS::NodeAllocator nodeAllocator(
        nodeBitmap,
        nodeTable
    );

    /*
     * Root's Node struct is written directly below, bypassing
     * NodeAllocator. Without this, nodeBitmap never learns that
     * index 0 (RootNode) is taken, and the very next
     * nodeManager.create() call hands out index 0 as "free" --
     * silently overwriting root's own Node with the new node's data.
     * The real formatter (formatter.cpp) does exactly this same
     * setBit(nodeBitmap, RootNode) before allocating anything else;
     * this test previously skipped it.
     */
    nodeBitmap.set(TalonsFS::RootNode);

    TalonsFS::NodeManager nodeManager(
        nodeAllocator,
        nodeTable
    );

    TalonsFS::Node root{};
    root.type = TalonsFS::NodeType::Directory;

    nodeTable.write(
        TalonsFS::RootNode,
        root
    );

    /*
     * The real formatter and kernel/filesystem.cpp ALWAYS create the
     * root node through nodeManager->create(), which marks it
     * allocated in the node bitmap as a side effect. This test writes
     * the root node directly into the table instead (to keep the
     * fixture minimal), which skips that bookkeeping: bit 0 of
     * nodeBitmap is left clear. The very next nodeManager.create()
     * call below then legitimately hands out node 0 -- the "first
     * free" node, since nothing marked it otherwise -- colliding with
     * and overwriting the root node just constructed above.
     *
     * Mark it allocated explicitly to match the invariant every real
     * code path maintains.
     */
    nodeBitmap.set(
        TalonsFS::RootNode
    );

    int64_t fileNode =
        nodeManager.create(
            TalonsFS::NodeType::File
        );

    if (fileNode < 0) {
        std::cout << "File creation failed\n";
        return 1;
    }

    TalonsFS::NodeDataManager nodeDataManager(
        blockAllocator,
        nodeTable
    );

    TalonsFS::DataBlockManager dataBlocks(
        device
    );

    TalonsFS::Directory directory(
        dataBlocks,
        nodeTable,
        nodeDataManager
    );

    TalonsFS::DirectoryEntry entry{};

    entry.node =
        static_cast<uint64_t>(fileNode);

    entry.type = 1;

    std::strncpy(
        entry.name,
        "hello.txt",
        TalonsFS::MaxFilenameLength - 1
    );

    if (!directory.add(
            TalonsFS::RootNode,
            entry
        ))
    {
        std::cout << "Directory entry creation failed\n";
        return 1;
    }

    TalonsFS::DirectoryEntry found{};

    if (!directory.find(
            TalonsFS::RootNode,
            "hello.txt",
            found
        ))
    {
        std::cout << "Directory lookup failed\n";
        return 1;
    }

    std::cout << "File node: "
              << fileNode
              << '\n';

    std::cout << "Entry added: 1\n";

    std::cout << "Entry found: 1\n";

    std::cout << "Found name: "
              << found.name
              << '\n';

    std::cout << "Found node: "
              << found.node
              << '\n';

    return 0;
}
