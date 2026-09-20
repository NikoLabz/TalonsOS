// NOTE: host-only. Uses <vector>/<algorithm>/<limits> or other hosted-C++/libc
// features and cannot compile in the freestanding kernel build. Used by the
// TalonsFS test suite and host-side tools (e.g. mkTalonsFS) only — never
// #include this from kernel/ code.

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

#include "../include/talonsfs/directory.h"

namespace TalonsFS {

Directory::Directory(
    DataBlockManager& dataBlocks,
    NodeTable& nodeTable,
    NodeDataManager& nodeData
)
    : dataBlocks_(dataBlocks),
      nodeTable_(nodeTable),
      nodeData_(nodeData)
{
}

bool Directory::add(
    uint64_t directoryNode,
    const DirectoryEntry& entry
)
{
    Node node = nodeTable_.read(directoryNode);

    if (node.type != NodeType::Directory) {
        return false;
    }

    if (entry.name[0] == '\0') {
        return false;
    }

    if (std::memchr(
            entry.name,
            '\0',
            MaxFilenameLength
        ) == nullptr)
    {
        return false;
    }

    constexpr uint64_t entrySize =
        sizeof(DirectoryEntry);

    const uint64_t blockSize =
        dataBlocks_.getBlockSize();

    if (blockSize == 0) {
        return false;
    }

    if (node.size >
        std::numeric_limits<uint64_t>::max() -
            entrySize)
    {
        return false;
    }

    const uint64_t newSize =
        node.size + entrySize;

    const uint64_t requiredBlocks =
        (newSize + blockSize - 1) / blockSize;


    while (node.dataBlocks < requiredBlocks) {

        if (!nodeData_.allocateBlock(directoryNode)) {
            return false;
        }

        node = nodeTable_.read(directoryNode);
    }


    const uint64_t offset = node.size;

    const uint8_t* source =
        reinterpret_cast<const uint8_t*>(&entry);

    uint64_t remaining = entrySize;
    uint64_t currentOffset = offset;


    while (remaining > 0) {

        const uint64_t blockIndex =
            currentOffset / blockSize;

        const uint64_t offsetInBlock =
            currentOffset % blockSize;


        const uint64_t blockNumber =
            node.dataStart + blockIndex;


        std::vector<uint8_t> blockBuffer(
            blockSize
        );


        if (!dataBlocks_.read(
                blockNumber,
                blockBuffer.data()
            ))
        {
            return false;
        }


        const uint64_t space =
            blockSize - offsetInBlock;


        const uint64_t bytesToCopy =
            std::min(
                remaining,
                space
            );


        std::memcpy(
            blockBuffer.data() + offsetInBlock,
            source,
            bytesToCopy
        );


        if (!dataBlocks_.write(
                blockNumber,
                blockBuffer.data()
            ))
        {
            return false;
        }


        source += bytesToCopy;
        currentOffset += bytesToCopy;
        remaining -= bytesToCopy;
    }


    node.size = newSize;


    nodeTable_.write(
        directoryNode,
        node
    );


    return true;
}


bool Directory::find(
    uint64_t directoryNode,
    const char* name,
    DirectoryEntry& result
) const
{
    if (name == nullptr || name[0] == '\0') {
        return false;
    }


    Node node =
        nodeTable_.read(directoryNode);


    if (node.type != NodeType::Directory) {
        return false;
    }


    constexpr uint64_t entrySize =
        sizeof(DirectoryEntry);


    if (node.size % entrySize != 0) {
        return false;
    }


    const uint64_t blockSize =
        dataBlocks_.getBlockSize();


    if (blockSize == 0) {
        return false;
    }


    const uint64_t entryCount =
        node.size / entrySize;


    for (uint64_t entryIndex = 0;
         entryIndex < entryCount;
         ++entryIndex)
    {
        const uint64_t offset =
            entryIndex * entrySize;


        std::vector<uint8_t> entryBuffer(
            entrySize
        );


        uint64_t remaining = entrySize;
        uint64_t currentOffset = offset;

        uint8_t* destination =
            entryBuffer.data();


        while (remaining > 0) {

            const uint64_t blockIndex =
                currentOffset / blockSize;


            const uint64_t offsetInBlock =
                currentOffset % blockSize;


            if (blockIndex >= node.dataBlocks) {
                return false;
            }


            const uint64_t blockNumber =
                node.dataStart + blockIndex;


            std::vector<uint8_t> blockBuffer(
                blockSize
            );


            if (!dataBlocks_.read(
                    blockNumber,
                    blockBuffer.data()
                ))
            {
                return false;
            }


            const uint64_t available =
                blockSize - offsetInBlock;


            const uint64_t bytesToCopy =
                std::min(
                    remaining,
                    available
                );


            std::memcpy(
                destination,
                blockBuffer.data() + offsetInBlock,
                bytesToCopy
            );


            destination += bytesToCopy;
            currentOffset += bytesToCopy;
            remaining -= bytesToCopy;
        }


        DirectoryEntry entry{};


        std::memcpy(
            &entry,
            entryBuffer.data(),
            sizeof(DirectoryEntry)
        );


        if (std::strncmp(
                entry.name,
                name,
                MaxFilenameLength
            ) == 0)
        {
            result = entry;
            return true;
        }
    }


    return false;
}


std::vector<DirectoryEntry> Directory::list(
    uint64_t directoryNode
) const
{
    std::vector<DirectoryEntry> entries;

    Node node =
        nodeTable_.read(directoryNode);


    if (node.type != NodeType::Directory) {
        return entries;
    }


    constexpr uint64_t entrySize =
        sizeof(DirectoryEntry);


    const uint64_t blockSize =
        dataBlocks_.getBlockSize();


    if (blockSize == 0 ||
        node.size % entrySize != 0)
    {
        return entries;
    }


    const uint64_t entryCount =
        node.size / entrySize;


    for (uint64_t i = 0;
         i < entryCount;
         ++i)
    {
        const uint64_t offset =
            i * entrySize;


        std::vector<uint8_t> buffer(
            entrySize
        );


        uint64_t remaining = entrySize;
        uint64_t currentOffset = offset;

        uint8_t* destination =
            buffer.data();


        while (remaining > 0) {

            const uint64_t blockIndex =
                currentOffset / blockSize;


            const uint64_t offsetInBlock =
                currentOffset % blockSize;


            if (blockIndex >= node.dataBlocks) {
                return {};
            }


            const uint64_t blockNumber =
                node.dataStart + blockIndex;


            std::vector<uint8_t> blockBuffer(
                blockSize
            );


            if (!dataBlocks_.read(
                    blockNumber,
                    blockBuffer.data()
                ))
            {
                return {};
            }


            const uint64_t available =
                blockSize - offsetInBlock;


            const uint64_t bytesToCopy =
                std::min(
                    remaining,
                    available
                );


            std::memcpy(
                destination,
                blockBuffer.data() + offsetInBlock,
                bytesToCopy
            );


            destination += bytesToCopy;
            currentOffset += bytesToCopy;
            remaining -= bytesToCopy;
        }


        DirectoryEntry entry{};


        std::memcpy(
            &entry,
            buffer.data(),
            sizeof(DirectoryEntry)
        );


        if (entry.name[0] != '\0') {
            entries.push_back(entry);
        }
    }


    return entries;
}

}
