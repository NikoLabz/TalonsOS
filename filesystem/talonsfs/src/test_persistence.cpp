#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

#include "../include/talonsfs/file_block_device.h"
#include "../include/talonsfs/filesystem.h"

int main()
{
    constexpr uint64_t totalBlocks = 4096;
    constexpr uint32_t blockSize = 4096;

    const std::string image = "build/talonsfs_persistence.img";

    std::cout << "=== TalonsFS Persistence Test ===\n\n";

    /*
     * ============================================================
     * STEP 1: FORMAT AND WRITE
     * ============================================================
     */

    {
        std::cout << "[1] Creating filesystem image...\n";

        TalonsFS::FileBlockDevice device(
            image,
            totalBlocks,
            blockSize
        );

        if (!device.isOpen()) {
            std::cout << "FAILED: Could not open image\n";
            return 1;
        }

        TalonsFS::FileSystem fs(device);

        std::cout << "[2] Formatting...\n";

        if (!fs.format()) {
            std::cout << "FAILED: Format failed\n";
            return 1;
        }

        std::cout << "[3] Creating directory /test...\n";

        if (!fs.createDirectory("/test")) {
            std::cout << "FAILED: Could not create /test\n";
            return 1;
        }

        std::cout << "[4] Creating file /test/hello.txt...\n";

        if (!fs.createFile("/test/hello.txt")) {
            std::cout << "FAILED: Could not create file\n";
            return 1;
        }

        const std::string message =
            "Hello from TalonsFS persistence!";

        std::vector<uint8_t> data(
            message.begin(),
            message.end()
        );

        std::cout << "[5] Writing file...\n";

        if (!fs.writeFile(
                "/test/hello.txt",
                data
            ))
        {
            std::cout << "FAILED: Could not write file\n";
            return 1;
        }

        std::cout << "[6] Syncing filesystem...\n";

        if (!fs.sync()) {
            std::cout << "FAILED: Sync failed\n";
            return 1;
        }

        std::cout << "[7] Unmounting filesystem...\n";

        if (!fs.unmount()) {
            std::cout << "FAILED: Unmount failed\n";
            return 1;
        }

        std::cout << "WRITE PHASE PASSED\n\n";
    }

    /*
     * ============================================================
     * STEP 2: REMOUNT EXISTING IMAGE
     * ============================================================
     */

    {
        std::cout << "[8] Reopening filesystem image...\n";

        TalonsFS::FileBlockDevice device(
            image,
            totalBlocks,
            blockSize
        );

        if (!device.isOpen()) {
            std::cout << "FAILED: Could not reopen image\n";
            return 1;
        }

        TalonsFS::FileSystem fs(device);

        std::cout << "[9] Mounting existing filesystem...\n";

        if (!fs.mount()) {
            std::cout << "FAILED: Mount failed\n";
            return 1;
        }

        std::cout << "[10] Checking /test...\n";

        if (!fs.exists("/test")) {
            std::cout << "FAILED: /test does not exist\n";
            return 1;
        }

        std::cout << "[11] Checking /test/hello.txt...\n";

        if (!fs.exists("/test/hello.txt")) {
            std::cout << "FAILED: File does not exist\n";
            return 1;
        }

        std::vector<uint8_t> data;

        std::cout << "[12] Reading file...\n";

        if (!fs.readFile(
                "/test/hello.txt",
                data
            ))
        {
            std::cout << "FAILED: Could not read file\n";
            return 1;
        }

        std::string message(
            data.begin(),
            data.end()
        );

        std::cout << "[13] File contents:\n";
        std::cout << "      " << message << "\n";

        const std::string expected =
            "Hello from TalonsFS persistence!";

        if (message != expected) {
            std::cout << "FAILED: File contents do not match\n";
            return 1;
        }

        std::cout << "\nREAD/PERSISTENCE PHASE PASSED\n";
    }

    /*
     * ============================================================
     * RESULT
     * ============================================================
     */

    std::cout << "\n=== Result ===\n";
    std::cout << "TalonsFS persistence test PASSED.\n";

    return 0;
}
