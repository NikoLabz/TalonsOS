#include <iostream>
#include <cstdint>

#include "../include/talonsfs/bitmap.h"

int main()
{
    constexpr uint64_t nodeCount = 65536;

    uint8_t data[8192] = {};

    TalonsFS::Bitmap nodeBitmap(data, nodeCount);

    nodeBitmap.set(0);

    std::cout << "Root node allocated: "
              << nodeBitmap.get(0)
              << '\n';

    std::cout << "Node 1 allocated: "
              << nodeBitmap.get(1)
              << '\n';

    std::cout << "Node 100 allocated: "
              << nodeBitmap.get(100)
              << '\n';

    return 0;
}
