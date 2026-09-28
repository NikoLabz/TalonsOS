#include <iostream>
#include <cstdint>

#include "../include/talonsfs/bitmap.h"
#include "../include/talonsfs/node_allocator.h"
#include "../include/talonsfs/node_table.h"
#include "../include/talonsfs/node.h"

int main()
{
    constexpr uint64_t nodeCount = 16;

    uint8_t data[2] = {};

    TalonsFS::Bitmap bitmap(data, nodeCount);

    bitmap.set(0);

    // NodeAllocator only tracks which node indices are in use (via
    // the bitmap); the table is not touched by allocate()/free(), but
    // the constructor requires a real NodeTable to construct against.
    uint8_t nodeTableData[
        nodeCount * sizeof(TalonsFS::Node)
    ] = {};

    TalonsFS::NodeTable nodeTable(
        nodeTableData,
        nodeCount
    );

    TalonsFS::NodeAllocator allocator(
        bitmap,
        nodeTable
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
