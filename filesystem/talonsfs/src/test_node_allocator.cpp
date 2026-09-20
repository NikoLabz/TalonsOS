#include <iostream>
#include <cstdint>

#include "../include/talonsfs/bitmap.h"
#include "../include/talonsfs/node_allocator.h"

int main()
{
    constexpr uint64_t nodeCount = 16;

    uint8_t data[2] = {};

    TalonsFS::Bitmap bitmap(data, nodeCount);

    bitmap.set(0);

    TalonsFS::NodeAllocator allocator(
        bitmap,
        nodeCount
    );

    int64_t first = allocator.allocate();
    int64_t second = allocator.allocate();

    std::cout << "First allocated node: "
              << first
              << '\n';

    std::cout << "Second allocated node: "
              << second
              << '\n';

    allocator.free(1);

    int64_t third = allocator.allocate();

    std::cout << "After freeing node 1: "
              << third
              << '\n';

    return 0;
}
