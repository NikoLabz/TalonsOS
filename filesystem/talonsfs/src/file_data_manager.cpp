#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

#include "../include/talonsfs/file_data_manager.h"
#include "../include/talonsfs/time.h"

namespace TalonsFS {

FileDataManager::FileDataManager(
    DataBlockManager& dataBlocks,
    NodeTable& nodeTable,
    uint64_t dataStart,
    uint64_t totalBlocks
)
    : dataBlocks_(dataBlocks),
      nodeTable_(nodeTable),
      dataStart_(dataStart),
      totalBlocks_(totalBlocks)
{
}

bool FileDataManager::write(
    uint64_t nodeIndex,
    const void* buffer,
    uint64_t size
)
{
    Node node = nodeTable_.read(nodeIndex);

    if (node.type != NodeType::File) {
        return false;
    }

    if (size > 0 && buffer == nullptr) {
        return false;
    }

    const uint64_t blockSize =
        dataBlocks_.getBlockSize();

    if (blockSize == 0) {
        return false;
    }
if (size == 0) {
    node.size = 0;
    node.modifiedTime = currentUnixTime();

    nodeTable_.write(
        nodeIndex,
        node
    );

    return true;
}

    if (node.dataBlocks == 0) {
        return false;
    }

    if (node.dataStart < dataStart_) {
        return false;
    }

    if (node.dataStart >= totalBlocks_) {
        return false;
    }

    if (node.dataBlocks >
        totalBlocks_ - node.dataStart)
    {
        return false;
    }

    if (node.dataBlocks >
        std::numeric_limits<uint64_t>::max() / blockSize)
    {
        return false;
    }

    const uint64_t capacity =
        node.dataBlocks * blockSize;

    if (size > capacity) {
        return false;
    }

    const uint8_t* source =
        static_cast<const uint8_t*>(buffer);

    std::vector<uint8_t> blockBuffer(
        blockSize
    );

    uint64_t remaining = size;

    for (uint64_t i = 0;
         i < node.dataBlocks && remaining > 0;
         ++i)
    {
        std::fill(
            blockBuffer.begin(),
            blockBuffer.end(),
            0
        );

        const uint64_t bytesToCopy =
            std::min<uint64_t>(
                remaining,
                blockSize
            );

        std::memcpy(
            blockBuffer.data(),
            source,
            bytesToCopy
        );

        if (!dataBlocks_.write(
                node.dataStart + i,
                blockBuffer.data()
            ))
        {
            return false;
        }

        source += bytesToCopy;
        remaining -= bytesToCopy;
    }

node.size = size;
node.modifiedTime = currentUnixTime();

nodeTable_.write(
    nodeIndex,
    node
);

    return true;
}

bool FileDataManager::read(
    uint64_t nodeIndex,
    void* buffer,
    uint64_t size
) const
{
    Node node = nodeTable_.read(nodeIndex);

    if (node.type != NodeType::File) {
        return false;
    }

    if (size > 0 && buffer == nullptr) {
        return false;
    }

    const uint64_t blockSize =
        dataBlocks_.getBlockSize();

    if (blockSize == 0) {
        return false;
    }

    if (size == 0) {
        return true;
    }

    if (node.dataBlocks == 0) {
        return false;
    }

    if (node.dataStart < dataStart_) {
        return false;
    }

    if (node.dataStart >= totalBlocks_) {
        return false;
    }

    if (node.dataBlocks >
        totalBlocks_ - node.dataStart)
    {
        return false;
    }

    if (node.dataBlocks >
        std::numeric_limits<uint64_t>::max() / blockSize)
    {
        return false;
    }

    const uint64_t capacity =
        node.dataBlocks * blockSize;

    if (size > node.size || size > capacity) {
        return false;
    }

    uint8_t* destination =
        static_cast<uint8_t*>(buffer);

    std::vector<uint8_t> blockBuffer(
        blockSize
    );

    uint64_t remaining = size;

    for (uint64_t i = 0;
         i < node.dataBlocks && remaining > 0;
         ++i)
    {
        if (!dataBlocks_.read(
                node.dataStart + i,
                blockBuffer.data()
            ))
        {
            return false;
        }

        const uint64_t bytesToCopy =
            std::min<uint64_t>(
                remaining,
                blockSize
            );

        std::memcpy(
            destination,
            blockBuffer.data(),
            bytesToCopy
        );

        destination += bytesToCopy;
        remaining -= bytesToCopy;
    }

    return remaining == 0;
}

}
