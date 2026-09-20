// NOTE: host-only. Uses <vector>/<algorithm>/<limits> or other hosted-C++/libc
// features and cannot compile in the freestanding kernel build. Used by the
// TalonsFS test suite and host-side tools (e.g. mkTalonsFS) only — never
// #include this from kernel/ code.

#include "../include/talonsfs/memory_block_device.h"

namespace TalonsFS {

MemoryBlockDevice::MemoryBlockDevice(
    uint64_t blockCount,
    uint32_t blockSize
)
    : blockCount(blockCount),
      blockSize(blockSize)
{
    storage = new uint8_t[blockCount * blockSize]();
}

MemoryBlockDevice::~MemoryBlockDevice()
{
    delete[] storage;
}

bool MemoryBlockDevice::readBlock(
    uint64_t blockNumber,
    void* buffer
)
{
    if (blockNumber >= blockCount) {
        return false;
    }

    uint64_t offset = blockNumber * blockSize;

    for (uint32_t i = 0; i < blockSize; i++) {
        static_cast<uint8_t*>(buffer)[i] =
            storage[offset + i];
    }

    return true;
}

bool MemoryBlockDevice::writeBlock(
    uint64_t blockNumber,
    const void* buffer
)
{
    if (blockNumber >= blockCount) {
        return false;
    }

    uint64_t offset = blockNumber * blockSize;

    for (uint32_t i = 0; i < blockSize; i++) {
        storage[offset + i] =
            static_cast<const uint8_t*>(buffer)[i];
    }

    return true;
}

uint64_t MemoryBlockDevice::getBlockCount() const
{
    return blockCount;
}

uint32_t MemoryBlockDevice::getBlockSize() const
{
    return blockSize;
}

}
