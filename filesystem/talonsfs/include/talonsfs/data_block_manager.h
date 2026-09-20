#pragma once

#include <cstdint>

#include "block_device.h"

namespace TalonsFS {

class DataBlockManager {
public:
    explicit DataBlockManager(BlockDevice& device);

    bool read(
        uint64_t blockNumber,
        void* buffer
    );

    bool write(
        uint64_t blockNumber,
        const void* buffer
    );
    
    uint32_t getBlockSize() const;

private:
    BlockDevice& device_;
};

}
