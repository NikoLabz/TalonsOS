#include <iostream>
#include <cstdint>

#include "../include/talonsfs/bitmap.h"
#include "../include/talonsfs/block_allocator.h"

int main()
{
    constexpr uint64_t totalBlocks = 32;
    constexpr uint64_t dataStart = 20;

    uint8_t bitmapData[4] = {};

    TalonsFS::Bitmap bitmap(
        bitmapData,
        totalBlocks
    );

    TalonsFS::BlockAllocator allocator(
        bitmap,
        totalBlocks,
        dataStart
    );

    int64_t block = allocator.allocate();

    std::cout << "Allocated block: "
              << block
              << '\n';

    allocator.free(5);

    std::cout << "Block 5 allocated after invalid free: "
              << bitmap.get(5)
              << '\n';

    allocator.free(19);

    std::cout << "Block 19 allocated after invalid free: "
              << bitmap.get(19)
              << '\n';

    return 0;
}
