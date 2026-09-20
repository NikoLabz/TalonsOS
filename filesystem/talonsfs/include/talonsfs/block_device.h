#pragma once

#include <cstdint>

namespace TalonsFS {

class BlockDevice {
public:
    virtual ~BlockDevice() = default;

    virtual bool readBlock(
        uint64_t blockNumber,
        void* buffer
    ) = 0;

    virtual bool writeBlock(
        uint64_t blockNumber,
        const void* buffer
    ) = 0;

    virtual uint64_t getBlockCount() const = 0;

    virtual uint32_t getBlockSize() const = 0;
};

}


