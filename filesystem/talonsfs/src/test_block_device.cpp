#include <cstdint>
#include <iostream>

#include "../include/talonsfs/memory_block_device.h"

int main()
{
    TalonsFS::MemoryBlockDevice device(10, 4096);

    uint8_t writeData[4096] = {};
    uint8_t readData[4096] = {};

    writeData[0] = 42;

    device.writeBlock(3, writeData);
    device.readBlock(3, readData);

    std::cout << "Written: "
              << static_cast<int>(writeData[0])
              << '\n';

    std::cout << "Read: "
              << static_cast<int>(readData[0])
              << '\n';

    return 0;
}
