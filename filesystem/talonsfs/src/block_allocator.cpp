#include "../include/talonsfs/block_allocator.h"

namespace TalonsFS {

BlockAllocator::BlockAllocator(
    Bitmap& bitmap,
    uint64_t totalBlocks,
    uint64_t dataStart
)
    : bitmap_(bitmap),
      totalBlocks_(totalBlocks),
      dataStart_(dataStart)
{
}

int64_t BlockAllocator::allocate()
{
    for (uint64_t block = dataStart_;
         block < totalBlocks_;
         ++block)
    {
        if (!bitmap_.get(block)) {
            bitmap_.set(block);
            return static_cast<int64_t>(block);
        }
    }

    return -1;
}

bool BlockAllocator::allocateSpecific(uint64_t block)
{
    if (block < dataStart_ || block >= totalBlocks_) {
        return false;
    }

    if (bitmap_.get(block)) {
        return false;
    }

    bitmap_.set(block);
    return true;
}

void BlockAllocator::free(uint64_t block)
{
    if (block < dataStart_ || block >= totalBlocks_) {
        return;
    }

    bitmap_.clear(block);
}

}
