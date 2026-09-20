#pragma once

#include <cstdint>

#include "bitmap.h"

namespace TalonsFS {

class BlockAllocator {
public:
    BlockAllocator(
        Bitmap& bitmap,
        uint64_t totalBlocks,
        uint64_t dataStart
    );

    int64_t allocate();

    bool allocateSpecific(uint64_t block);

    void free(uint64_t block);

private:
    Bitmap& bitmap_;
    uint64_t totalBlocks_;
    uint64_t dataStart_;
};

}
