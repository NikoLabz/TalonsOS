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

    int64_t first = allocator.allocate();
    int64_t second = allocator.allocate();

    std::cout << "First allocated block: "
              << first
              << '\n';

    std::cout << "Second allocated block: "
              << second
              << '\n';

    allocator.free(
        static_cast<uint64_t>(first)
    );

    int64_t third = allocator.allocate();

    std::cout << "After freeing first block: "
              << third
              << '\n';

    return 0;
}
