#pragma once

#include <cstdint>

#include "block_device.h"

namespace TalonsFS {

class MemoryBlockDevice : public BlockDevice {
private:
    uint8_t* storage;
    uint64_t blockCount;
    uint32_t blockSize;

public:
    MemoryBlockDevice(uint64_t blockCount, uint32_t blockSize);
    ~MemoryBlockDevice() override;

    bool readBlock(
        uint64_t blockNumber,
        void* buffer
    ) override;

    bool writeBlock(
        uint64_t blockNumber,
        const void* buffer
    ) override;

    uint64_t getBlockCount() const override;

    uint32_t getBlockSize() const override;
};

}
