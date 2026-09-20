#include <cstring>
#include <iostream>

#include "../include/talonsfs/file_block_device.h"

int main()
{
    constexpr uint64_t blockCount = 32;
    constexpr uint32_t blockSize = 4096;

    const char* imagePath =
        "talons_test.img";

    const char message[] =
        "TalonsOS survives.";

    /*
     * First session: create the image
     * and write data to block 20.
     */

    {
        TalonsFS::FileBlockDevice device(
            imagePath,
            blockCount,
            blockSize
        );

        if (!device.isOpen()) {
            std::cout << "First open failed\n";
            return 1;
        }

        char writeBuffer[blockSize] = {};

        std::memcpy(
            writeBuffer,
            message,
            sizeof(message)
        );

        if (!device.writeBlock(
                20,
                writeBuffer
            ))
        {
            std::cout << "Write failed\n";
            return 1;
        }

        std::cout << "Write successful: 1\n";
    }

    /*
     * Second session: reopen the image
     * and read the same block.
     */

    {
        TalonsFS::FileBlockDevice device(
            imagePath,
            blockCount,
            blockSize
        );

        if (!device.isOpen()) {
            std::cout << "Second open failed\n";
            return 1;
        }

        char readBuffer[blockSize] = {};

        if (!device.readBlock(
                20,
                readBuffer
            ))
        {
            std::cout << "Read failed\n";
            return 1;
        }

        std::cout << "Read successful: 1\n";

        std::cout << "Data: "
                  << readBuffer
                  << '\n';

        if (std::strcmp(
                readBuffer,
                message
            ) != 0)
        {
            std::cout << "Data mismatch\n";
            return 1;
        }

        std::cout << "Persistence test: 1\n";
    }

    return 0;
}
