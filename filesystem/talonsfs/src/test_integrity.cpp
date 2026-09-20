#include <iostream>
#include <cstdint>

#include "../include/talonsfs/bitmap.h"
#include "../include/talonsfs/block_device.h"
#include "../include/talonsfs/directory_entry.h"
#include "../include/talonsfs/memory_block_device.h"
#include "../include/talonsfs/node_table.h"
#include "../include/talonsfs/integrity.h"

int main()
{
    constexpr uint64_t nodeCount = 4;
    constexpr uint64_t totalBlocks = 32;
    constexpr uint64_t dataStart = 20;
    constexpr uint32_t blockSize = 512;

    uint8_t nodeBitmapData[1] = {};
    uint8_t blockBitmapData[4] = {};

    TalonsFS::Bitmap nodeBitmap(
        nodeBitmapData,
        nodeCount
    );

    TalonsFS::Bitmap blockBitmap(
        blockBitmapData,
        totalBlocks
    );

    uint8_t nodeData[
        nodeCount * sizeof(TalonsFS::Node)
    ] = {};

    TalonsFS::NodeTable nodeTable(
        nodeData,
        nodeCount
    );

    TalonsFS::Node root{};
    root.type = TalonsFS::NodeType::Directory;

    nodeBitmap.set(0);

    nodeTable.write(
        0,
        root
    );

    for (uint64_t block = 0;
         block < dataStart;
         ++block)
    {
        blockBitmap.set(block);
    }

    if (!TalonsFS::validateNodeConsistency(
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Valid node metadata: FAILED\n";

        return 1;
    }

    std::cout
        << "Valid node metadata: PASSED\n";

    nodeBitmap.set(1);

    if (TalonsFS::validateNodeConsistency(
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Allocated/free mismatch: FAILED\n";

        return 1;
    }

    std::cout
        << "Allocated/free mismatch: PASSED\n";

    nodeBitmap.clear(1);

    TalonsFS::Node invalid{};
    invalid.type = TalonsFS::NodeType::File;

    nodeTable.write(
        1,
        invalid
    );

    if (TalonsFS::validateNodeConsistency(
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Free/allocated mismatch: FAILED\n";

        return 1;
    }

    std::cout
        << "Free/allocated mismatch: PASSED\n";

    TalonsFS::Node invalidType{};
    invalidType.type =
        static_cast<TalonsFS::NodeType>(255);

    nodeBitmap.set(1);

    nodeTable.write(
        1,
        invalidType
    );

    if (TalonsFS::validateNodeConsistency(
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Invalid node type: FAILED\n";

        return 1;
    }

    std::cout
        << "Invalid node type: PASSED\n";

    nodeBitmap.clear(1);

    TalonsFS::Node invalidFreeNode{};
    invalidFreeNode.type =
        TalonsFS::NodeType::Free;

    invalidFreeNode.dataStart = 20;
    invalidFreeNode.dataBlocks = 1;

    nodeTable.write(
        1,
        invalidFreeNode
    );

    if (TalonsFS::validateNodeConsistency(
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Free node with data: FAILED\n";

        return 1;
    }

    std::cout
        << "Free node with data: PASSED\n";

    nodeBitmap.set(1);

    TalonsFS::Node invalidStart{};
    invalidStart.type =
        TalonsFS::NodeType::File;

    invalidStart.dataStart = 19;
    invalidStart.dataBlocks = 1;

    nodeTable.write(
        1,
        invalidStart
    );

    if (TalonsFS::validateNodeConsistency(
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Data before dataStart: FAILED\n";

        return 1;
    }

    std::cout
        << "Data before dataStart: PASSED\n";

    TalonsFS::Node invalidEnd{};
    invalidEnd.type =
        TalonsFS::NodeType::File;

    invalidEnd.dataStart = 31;
    invalidEnd.dataBlocks = 2;

    nodeTable.write(
        1,
        invalidEnd
    );

    if (TalonsFS::validateNodeConsistency(
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Data beyond filesystem: FAILED\n";

        return 1;
    }

    std::cout
        << "Data beyond filesystem: PASSED\n";

    TalonsFS::Node file{};
    file.type = TalonsFS::NodeType::File;
    file.dataStart = 20;
    file.dataBlocks = 2;

    nodeTable.write(
        1,
        file
    );

    blockBitmap.set(20);
    blockBitmap.set(21);

    if (!TalonsFS::validateBlockConsistency(
            blockBitmap,
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Valid block metadata: FAILED\n";

        return 1;
    }

    std::cout
        << "Valid block metadata: PASSED\n";

    blockBitmap.clear(5);

    if (TalonsFS::validateBlockConsistency(
            blockBitmap,
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Free metadata block: FAILED\n";

        return 1;
    }

    std::cout
        << "Free metadata block: PASSED\n";

    blockBitmap.set(5);

    blockBitmap.clear(21);

    if (TalonsFS::validateBlockConsistency(
            blockBitmap,
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Missing node block: FAILED\n";

        return 1;
    }

    std::cout
        << "Missing node block: PASSED\n";

    blockBitmap.set(21);

    TalonsFS::Node invalidRange{};
    invalidRange.type =
        TalonsFS::NodeType::File;

    invalidRange.dataStart = 31;
    invalidRange.dataBlocks = 2;

    nodeTable.write(
        1,
        invalidRange
    );

    blockBitmap.set(31);

    if (TalonsFS::validateBlockConsistency(
            blockBitmap,
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Node beyond filesystem: FAILED\n";

        return 1;
    }

    std::cout
        << "Node beyond filesystem: PASSED\n";

    nodeTable.write(
        1,
        file
    );

    nodeBitmap.set(2);

    TalonsFS::Node overlappingFile{};
    overlappingFile.type =
        TalonsFS::NodeType::File;

    overlappingFile.dataStart = 21;
    overlappingFile.dataBlocks = 2;

    nodeTable.write(
        2,
        overlappingFile
    );

    blockBitmap.set(22);

    if (TalonsFS::validateBlockConsistency(
            blockBitmap,
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Overlapping node blocks: FAILED\n";

        return 1;
    }

    std::cout
        << "Overlapping node blocks: PASSED\n";

    nodeBitmap.clear(2);

    nodeTable.write(
        2,
        TalonsFS::Node{}
    );

    blockBitmap.clear(22);

    blockBitmap.set(21);

    blockBitmap.set(25);

    if (TalonsFS::validateBlockConsistency(
            blockBitmap,
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        std::cout
            << "Unowned allocated block: FAILED\n";

        return 1;
    }

        std::cout
    << "Unowned allocated block: PASSED\n";

blockBitmap.clear(25);

/*
 * Bitmap consistency tests
 */

const uint64_t expectedFreeBlocks =
    totalBlocks - 23;

const uint64_t expectedFreeNodes =
    nodeCount - 2;

if (!TalonsFS::validateBitmapConsistency(
        blockBitmap,
        nodeBitmap,
        dataStart,
        totalBlocks,
        expectedFreeBlocks,
        expectedFreeNodes))
{
    std::cout
        << "Valid bitmap metadata: FAILED\n";

    return 1;
}

std::cout
    << "Valid bitmap metadata: PASSED\n";

if (TalonsFS::validateBitmapConsistency(
        blockBitmap,
        nodeBitmap,
        dataStart,
        totalBlocks,
        expectedFreeBlocks + 1,
        expectedFreeNodes))
{
    std::cout
        << "Incorrect free-block count: FAILED\n";

    return 1;
}

std::cout
    << "Incorrect free-block count: PASSED\n";

if (TalonsFS::validateBitmapConsistency(
        blockBitmap,
        nodeBitmap,
        dataStart,
        totalBlocks,
        expectedFreeBlocks,
        expectedFreeNodes + 1))
{
    std::cout
        << "Incorrect free-node count: FAILED\n";

    return 1;
}

std::cout
    << "Incorrect free-node count: PASSED\n";

uint8_t invalidBlockBitmapData[4] = {};

TalonsFS::Bitmap invalidBlockBitmap(
    invalidBlockBitmapData,
    31
);

if (TalonsFS::validateBitmapConsistency(
        invalidBlockBitmap,
        nodeBitmap,
        dataStart,
        totalBlocks,
        expectedFreeBlocks,
        expectedFreeNodes))
{
    std::cout
        << "Bitmap size mismatch: FAILED\n";

    return 1;
}

   std::cout
    << "Bitmap size mismatch: PASSED\n";

/*
 * Directory integrity tests
 */

    TalonsFS::MemoryBlockDevice device(
        totalBlocks,
        blockSize
    );

    TalonsFS::Node directory{};
    directory.type =
        TalonsFS::NodeType::Directory;

    directory.dataStart = 20;
    directory.dataBlocks = 1;

    nodeTable.write(
        0,
        directory
    );

    nodeBitmap.set(0);
    nodeBitmap.set(1);

    TalonsFS::Node directoryFile{};
    directoryFile.type =
        TalonsFS::NodeType::File;

    directoryFile.dataStart = 20;
    directoryFile.dataBlocks = 2;

    nodeTable.write(
        1,
        directoryFile
    );

    TalonsFS::DirectoryEntry entry{};

    entry.node = 1;
    entry.type =
        static_cast<uint8_t>(
            TalonsFS::NodeType::File
        );

    entry.name[0] = 'h';
    entry.name[1] = 'e';
    entry.name[2] = 'l';
    entry.name[3] = 'l';
    entry.name[4] = 'o';
    entry.name[5] = '.';
    entry.name[6] = 't';
    entry.name[7] = 'x';
    entry.name[8] = 't';
    entry.name[9] = '\0';

    directory.size =
        sizeof(TalonsFS::DirectoryEntry);

    nodeTable.write(
        0,
        directory
    );

    device.writeBlock(
        20,
        &entry
    );

    if (!TalonsFS::validateDirectoryConsistency(
            device,
            nodeBitmap,
            nodeTable))
    {
        std::cout
            << "Valid directory entry: FAILED\n";

        return 1;
    }

    std::cout
        << "Valid directory entry: PASSED\n";

blockBitmap.clear(31);

directory.dataStart = 22;

nodeTable.write(
    0,
    directory
);

blockBitmap.set(22);

device.writeBlock(
    22,
    &entry
);

/*
 * Unified metadata integrity tests
 */

const uint64_t unifiedExpectedFreeBlocks =
    totalBlocks - 23;

const uint64_t unifiedExpectedFreeNodes =
    nodeCount - 2;

if (!TalonsFS::validateMetadataConsistency(
        device,
        blockBitmap,
        nodeBitmap,
        nodeTable,
        dataStart,
        totalBlocks,
        unifiedExpectedFreeBlocks,
        unifiedExpectedFreeNodes))
{
    std::cout
        << "Valid metadata: FAILED\n";

    return 1;
}

std::cout
    << "Valid metadata: PASSED\n";

TalonsFS::Node savedNode =
    nodeTable.read(1);

TalonsFS::Node corruptedNode =
    savedNode;

corruptedNode.type =
    static_cast<TalonsFS::NodeType>(255);

nodeTable.write(
    1,
    corruptedNode
);

if (TalonsFS::validateMetadataConsistency(
        device,
        blockBitmap,
        nodeBitmap,
        nodeTable,
        dataStart,
        totalBlocks,
        unifiedExpectedFreeBlocks,
        unifiedExpectedFreeNodes))
{
    std::cout
        << "Corrupted node metadata: FAILED\n";

    return 1;
}

std::cout
    << "Corrupted node metadata: PASSED\n";

nodeTable.write(
    1,
    savedNode
);

blockBitmap.clear(20);

if (TalonsFS::validateMetadataConsistency(
        device,
        blockBitmap,
        nodeBitmap,
        nodeTable,
        dataStart,
        totalBlocks,
        unifiedExpectedFreeBlocks,
        unifiedExpectedFreeNodes))
{
    std::cout
        << "Corrupted block metadata: FAILED\n";

    return 1;
}

std::cout
    << "Corrupted block metadata: PASSED\n";

blockBitmap.set(20);

if (TalonsFS::validateMetadataConsistency(
        device,
        blockBitmap,
        nodeBitmap,
        nodeTable,
        dataStart,
        totalBlocks,
        unifiedExpectedFreeBlocks + 1,
        unifiedExpectedFreeNodes))
{
    std::cout
        << "Corrupted bitmap counts: FAILED\n";

    return 1;
}

std::cout
    << "Corrupted bitmap counts: PASSED\n";

if (!TalonsFS::validateMetadataConsistency(
        device,
        blockBitmap,
        nodeBitmap,
        nodeTable,
        dataStart,
        totalBlocks,
        unifiedExpectedFreeBlocks,
        unifiedExpectedFreeNodes))
{
    std::cout
        << "Restored metadata: FAILED\n";

    return 1;
}

std::cout
    << "Restored metadata: PASSED\n";

/*
 * Node reachability tests
 */

TalonsFS::Node orphanNode{};
orphanNode.type =
    TalonsFS::NodeType::File;

nodeBitmap.set(2);

nodeTable.write(
    2,
    orphanNode
);

if (TalonsFS::validateNodeReachability(
        device,
        nodeBitmap,
        nodeTable))
{
    std::cout
        << "Unreachable allocated node: FAILED\n";

    return 1;
}

std::cout
    << "Unreachable allocated node: PASSED\n";

nodeBitmap.clear(2);

nodeTable.write(
    2,
    TalonsFS::Node{}
);

if (!TalonsFS::validateNodeReachability(
        device,
        nodeBitmap,
        nodeTable))
{
    std::cout
        << "Restored node reachability: FAILED\n";

    return 1;
}

std::cout
    << "Restored node reachability: PASSED\n";

    entry.node = 99;

    device.writeBlock(
        22,
        &entry
    );

    if (TalonsFS::validateDirectoryConsistency(
            device,
            nodeBitmap,
            nodeTable))
    {
        std::cout
            << "Nonexistent directory target: FAILED\n";

        return 1;
    }

    std::cout
        << "Nonexistent directory target: PASSED\n";

    entry.node = 2;

    device.writeBlock(
        20,
        &entry
    );

    if (TalonsFS::validateDirectoryConsistency(
            device,
            nodeBitmap,
            nodeTable))
    {
        std::cout
            << "Free directory target: FAILED\n";

        return 1;
    }

    std::cout
        << "Free directory target: PASSED\n";

    entry.node = 1;
    entry.type =
        static_cast<uint8_t>(
            TalonsFS::NodeType::Directory
        );

    device.writeBlock(
        20,
        &entry
    );

    if (TalonsFS::validateDirectoryConsistency(
            device,
            nodeBitmap,
            nodeTable))
    {
        std::cout
            << "Directory type mismatch: FAILED\n";

        return 1;
    }

    std::cout
        << "Directory type mismatch: PASSED\n";

    entry.type =
        static_cast<uint8_t>(
            TalonsFS::NodeType::File
        );

    for (uint32_t i = 0;
         i < TalonsFS::MaxFilenameLength;
         ++i)
    {
        entry.name[i] = 'x';
    }

    device.writeBlock(
        20,
        &entry
    );

    if (TalonsFS::validateDirectoryConsistency(
            device,
            nodeBitmap,
            nodeTable))
    {
        std::cout
            << "Unterminated filename: FAILED\n";

        return 1;
    }

    std::cout
        << "Unterminated filename: PASSED\n";

    entry.name[0] = 'x';
    entry.name[1] = '\0';

    directory.size =
        sizeof(TalonsFS::DirectoryEntry) - 1;

    nodeTable.write(
        0,
        directory
    );

    device.writeBlock(
        20,
        &entry
    );

    if (TalonsFS::validateDirectoryConsistency(
            device,
            nodeBitmap,
            nodeTable))
    {
        std::cout
            << "Invalid directory size: FAILED\n";

        return 1;
    }

    std::cout
        << "Invalid directory size: PASSED\n";

    directory.size =
    sizeof(TalonsFS::DirectoryEntry) *
    ((blockSize /
      sizeof(TalonsFS::DirectoryEntry)) + 1);

    nodeTable.write(
        0,
        directory
    );

    if (TalonsFS::validateDirectoryConsistency(
            device,
            nodeBitmap,
            nodeTable))
    {
        std::cout
            << "Too many directory entries: FAILED\n";

        return 1;
    }

    std::cout
        << "Too many directory entries: PASSED\n";

    return 0;
}
