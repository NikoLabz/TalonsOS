#include <vector>

#include "../include/talonsfs/layout.h"
#include "../include/talonsfs/integrity.h"
#include "../include/talonsfs/directory_entry.h"

namespace TalonsFS {

bool validateNodeConsistency(
    const Bitmap& nodeBitmap,
    const NodeTable& nodeTable,
    uint64_t dataStart,
    uint64_t totalBlocks
)
{
    for (uint64_t i = 0;
         i < nodeBitmap.size();
         ++i)
    {
        const bool allocated =
            nodeBitmap.get(i);

        const Node node =
            nodeTable.read(i);

        if (node.type != NodeType::Free &&
            node.type != NodeType::File &&
            node.type != NodeType::Directory)
        {
            return false;
        }

        if (allocated &&
            node.type == NodeType::Free)
        {
            return false;
        }

        if (!allocated &&
            node.type != NodeType::Free)
        {
            return false;
        }

        if (node.type == NodeType::Free)
        {
            if (node.dataStart != 0 ||
                node.dataBlocks != 0)
            {
                return false;
            }

            continue;
        }

        if (node.dataBlocks == 0)
        {
            if (node.dataStart != 0)
            {
                return false;
            }

            continue;
        }

        if (node.dataStart < dataStart)
        {
            return false;
        }

        if (node.dataStart >= totalBlocks)
        {
            return false;
        }

        if (node.dataBlocks >
            totalBlocks - node.dataStart)
        {
            return false;
        }
    }

    return true;
}

bool validateBlockConsistency(
    const Bitmap& blockBitmap,
    const Bitmap& nodeBitmap,
    const NodeTable& nodeTable,
    uint64_t dataStart,
    uint64_t totalBlocks
)
{
    if (blockBitmap.size() != totalBlocks)
    {
        return false;
    }

    if (dataStart > totalBlocks)
    {
        return false;
    }

    for (uint64_t block = 0;
         block < dataStart;
         ++block)
    {
        if (!blockBitmap.get(block))
        {
            return false;
        }
    }

    std::vector<uint8_t> claimedBlocks(
        totalBlocks,
        0
    );

    for (uint64_t nodeIndex = 0;
         nodeIndex < nodeBitmap.size();
         ++nodeIndex)
    {
        if (!nodeBitmap.get(nodeIndex))
        {
            continue;
        }

        const Node node =
            nodeTable.read(nodeIndex);

        if (node.dataBlocks == 0)
        {
            continue;
        }

        if (node.dataStart < dataStart ||
            node.dataStart >= totalBlocks)
        {
            return false;
        }

        if (node.dataBlocks >
            totalBlocks - node.dataStart)
        {
            return false;
        }

        for (uint64_t i = 0;
             i < node.dataBlocks;
             ++i)
        {
            const uint64_t block =
                node.dataStart + i;

            if (!blockBitmap.get(block))
            {
                return false;
            }

            if (claimedBlocks[block] != 0)
            {
                return false;
            }

            claimedBlocks[block] = 1;
        }
    }

    for (uint64_t block = dataStart;
         block < totalBlocks;
         ++block)
    {
        if (blockBitmap.get(block) &&
            claimedBlocks[block] == 0)
        {
            return false;
        }
    }

    return true;
}

bool validateDirectoryConsistency(
    BlockDevice& device,
    const Bitmap& nodeBitmap,
    const NodeTable& nodeTable
)
{
    const uint32_t blockSize =
        device.getBlockSize();

    if (blockSize == 0 ||
        blockSize < sizeof(DirectoryEntry))
    {
        return false;
    }

    std::vector<uint8_t> buffer(
        blockSize
    );

    for (uint64_t nodeIndex = 0;
         nodeIndex < nodeBitmap.size();
         ++nodeIndex)
    {
        if (!nodeBitmap.get(nodeIndex))
        {
            continue;
        }

        const Node directory =
            nodeTable.read(nodeIndex);

        if (directory.type != NodeType::Directory)
        {
            continue;
        }

        if (directory.size %
            sizeof(DirectoryEntry) != 0)
        {
            return false;
        }

        if (directory.dataBlocks == 0)
        {
            if (directory.size != 0)
            {
                return false;
            }

            continue;
        }

        const uint64_t entriesPerBlock =
            blockSize / sizeof(DirectoryEntry);

        const uint64_t entryCount =
            directory.size /
            sizeof(DirectoryEntry);

        for (uint64_t entryIndex = 0;
             entryIndex < entryCount;
             ++entryIndex)
        {
            const uint64_t blockIndex =
                entryIndex / entriesPerBlock;

            if (blockIndex >= directory.dataBlocks)
            {
                return false;
            }

            const uint64_t entryOffset =
                (entryIndex % entriesPerBlock) *
                sizeof(DirectoryEntry);

            const uint64_t block =
                directory.dataStart + blockIndex;

            if (!device.readBlock(
                    block,
                    buffer.data()))
            {
                return false;
            }

            DirectoryEntry entry{};

            __builtin_memcpy(
                &entry,
                buffer.data() + entryOffset,
                sizeof(DirectoryEntry)
            );

            if (entry.name[0] == '\0')
            {
                continue;
            }

            bool terminated = false;

            for (uint32_t i = 0;
                 i < MaxFilenameLength;
                 ++i)
            {
                if (entry.name[i] == '\0')
                {
                    terminated = true;
                    break;
                }
            }

            if (!terminated)
            {
                return false;
            }

            if (entry.node >= nodeBitmap.size())
            {
                return false;
            }

            if (!nodeBitmap.get(entry.node))
            {
                return false;
            }

            const Node target =
                nodeTable.read(entry.node);

            const uint8_t targetType =
                static_cast<uint8_t>(
                    target.type
                );

            if (entry.type != targetType)
            {
                return false;
            }
        }
    }

    return true;
}

bool validateNodeReachability(
    BlockDevice& device,
    const Bitmap& nodeBitmap,
    const NodeTable& nodeTable
)
{
    const uint32_t blockSize =
        device.getBlockSize();

    if (blockSize == 0 ||
        blockSize < sizeof(DirectoryEntry))
    {
        return false;
    }

    if (nodeBitmap.size() == 0 ||
        !nodeBitmap.get(RootNode))
    {
        return false;
    }

    std::vector<uint8_t> reachable(
        nodeBitmap.size(),
        0
    );

    std::vector<uint64_t> pending;
    pending.push_back(RootNode);

    while (!pending.empty())
    {
        const uint64_t nodeIndex =
            pending.back();

        pending.pop_back();

        if (nodeIndex >= nodeBitmap.size())
        {
            return false;
        }

        if (reachable[nodeIndex] != 0)
        {
            continue;
        }

        if (!nodeBitmap.get(nodeIndex))
        {
            return false;
        }

        reachable[nodeIndex] = 1;

        const Node node =
            nodeTable.read(nodeIndex);

        if (node.type == NodeType::File)
        {
            continue;
        }

        if (node.type != NodeType::Directory)
        {
            return false;
        }

        if (node.size %
            sizeof(DirectoryEntry) != 0)
        {
            return false;
        }

        if (node.dataBlocks == 0)
        {
            if (node.size != 0)
            {
                return false;
            }

            continue;
        }

        const uint64_t entriesPerBlock =
            blockSize / sizeof(DirectoryEntry);

        if (entriesPerBlock == 0)
        {
            return false;
        }

        const uint64_t entryCount =
            node.size /
            sizeof(DirectoryEntry);

        std::vector<uint8_t> buffer(
            blockSize
        );

        for (uint64_t entryIndex = 0;
             entryIndex < entryCount;
             ++entryIndex)
        {
            const uint64_t blockIndex =
                entryIndex / entriesPerBlock;

            if (blockIndex >= node.dataBlocks)
            {
                return false;
            }

            const uint64_t entryOffset =
                (entryIndex % entriesPerBlock) *
                sizeof(DirectoryEntry);

            const uint64_t block =
                node.dataStart + blockIndex;

            if (!device.readBlock(
                    block,
                    buffer.data()))
            {
                return false;
            }

            DirectoryEntry entry{};

            __builtin_memcpy(
                &entry,
                buffer.data() + entryOffset,
                sizeof(DirectoryEntry)
            );

            if (entry.name[0] == '\0')
            {
                continue;
            }

            bool terminated = false;

            for (uint32_t i = 0;
                 i < MaxFilenameLength;
                 ++i)
            {
                if (entry.name[i] == '\0')
                {
                    terminated = true;
                    break;
                }
            }

            if (!terminated)
            {
                return false;
            }

            if (entry.node >= nodeBitmap.size())
            {
                return false;
            }

            if (!nodeBitmap.get(entry.node))
            {
                return false;
            }

            const Node target =
                nodeTable.read(entry.node);

            const uint8_t targetType =
                static_cast<uint8_t>(
                    target.type
                );

            if (entry.type != targetType)
            {
                return false;
            }

            if (reachable[entry.node] == 0)
            {
                pending.push_back(entry.node);
            }
        }
    }

    for (uint64_t nodeIndex = 0;
         nodeIndex < nodeBitmap.size();
         ++nodeIndex)
    {
        if (nodeBitmap.get(nodeIndex) &&
            reachable[nodeIndex] == 0)
        {
            return false;
        }
    }

    return true;
}

bool validateBitmapConsistency(
    const Bitmap& blockBitmap,
    const Bitmap& nodeBitmap,
    uint64_t dataStart,
    uint64_t totalBlocks,
    uint64_t expectedFreeBlocks,
    uint64_t expectedFreeNodes
)
{
    if (blockBitmap.size() != totalBlocks)
    {
        return false;
    }

    if (dataStart > totalBlocks)
    {
        return false;
    }

    uint64_t allocatedBlocks = 0;

    for (uint64_t block = 0;
         block < totalBlocks;
         ++block)
    {
        if (blockBitmap.get(block))
        {
            ++allocatedBlocks;
        }
    }

    const uint64_t freeBlocks =
        totalBlocks - allocatedBlocks;

    if (freeBlocks != expectedFreeBlocks)
    {
        return false;
    }

    uint64_t allocatedNodes = 0;

    for (uint64_t node = 0;
         node < nodeBitmap.size();
         ++node)
    {
        if (nodeBitmap.get(node))
        {
            ++allocatedNodes;
        }
    }

    const uint64_t freeNodes =
        nodeBitmap.size() - allocatedNodes;

    if (freeNodes != expectedFreeNodes)
    {
        return false;
    }

    return true;
}

bool validateMetadataConsistency(
    BlockDevice& device,
    const Bitmap& blockBitmap,
    const Bitmap& nodeBitmap,
    const NodeTable& nodeTable,
    uint64_t dataStart,
    uint64_t totalBlocks,
    uint64_t expectedFreeBlocks,
    uint64_t expectedFreeNodes
)
{
    if (!validateNodeConsistency(
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        return false;
    }

    if (!validateBlockConsistency(
            blockBitmap,
            nodeBitmap,
            nodeTable,
            dataStart,
            totalBlocks))
    {
        return false;
    }

    if (!validateBitmapConsistency(
            blockBitmap,
            nodeBitmap,
            dataStart,
            totalBlocks,
            expectedFreeBlocks,
            expectedFreeNodes))
    {
        return false;
    }

    if (!validateDirectoryConsistency(
            device,
            nodeBitmap,
            nodeTable))
    {
        return false;
    }
    
    if (!validateNodeReachability(
        device,
        nodeBitmap,
        nodeTable))
{
    return false;
}

    return true;
}

}
