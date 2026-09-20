#include <iostream>
#include <cstring>
#include <cstdint>

#include "../include/talonsfs/memory_block_device.h"
#include "../include/talonsfs/data_block_manager.h"

int main()
{
    constexpr uint64_t blockCount = 32;
    constexpr uint32_t blockSize = 4096;

    TalonsFS::MemoryBlockDevice device(
        blockCount,
        blockSize
    );

    TalonsFS::DataBlockManager manager(device);

    const char message[] = "Hello, TalonsOS!";

    char writeBuffer[blockSize] = {};
    char readBuffer[blockSize] = {};

    std::memcpy(
        writeBuffer,
        message,
        sizeof(message)
    );

    bool writeSuccess =
        manager.write(20, writeBuffer);

    bool readSuccess =
        manager.read(20, readBuffer);

    std::cout << "Write successful: "
              << writeSuccess
              << '\n';

    std::cout << "Read successful: "
              << readSuccess
              << '\n';

    std::cout << "Data: "
              << readBuffer
              << '\n';

    return 0;
}

