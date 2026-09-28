#include <cstring>
#include <iostream>

#include "../include/talonsfs/file_block_device.h"

int main()
{
    constexpr uint64_t blockCount = 32;
    constexpr uint32_t blockSize = 4096;

    /*
     * Generated test artifact -- lives under build/ like every other
     * generated file in this repo, not the source tree. A prior
     * version of this test hard-coded "talons_test.img" in the repo
     * root; a stale image left over from an earlier run with
     * different blockCount/blockSize silently made every later run
     * fail, because FileBlockDevice correctly refuses to reuse an
     * existing image whose size does not match what was requested
     * (a fail-closed safety check, not a bug -- see
     * FileBlockDevice::FileBlockDevice). Using a build/ path avoids
     * that class of stale-fixture failure entirely.
     */
    const char* imagePath =
        "build/talons_test.img";

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
