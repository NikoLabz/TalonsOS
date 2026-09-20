#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <thread>
#include <chrono>

#include "../include/talonsfs/time.h"
#include "../include/talonsfs/file_block_device.h"
#include "../include/talonsfs/filesystem.h"

int main()
{
    constexpr uint64_t totalBlocks = 4096;
    constexpr uint32_t blockSize = 4096;

    const std::string imagePath =
        "talons_test.img";

    const std::string directoryPath =
        "/TheFridgeIsProbablyFull";

    const std::string filePath =
        "/TheFridgeIsProbablyFull/hello.txt";

    uint64_t originalCreatedTime = 0;
    uint64_t modifiedTime = 0;

    const std::string message =
        "Hello, TalonsOS!";

    std::remove(imagePath.c_str());

    /*
     * First session.
     */

    {
        TalonsFS::FileBlockDevice device(
            imagePath,
            totalBlocks,
            blockSize
        );

        if (!device.isOpen()) {
            std::cout << "Device open failed\n";
            return 1;
        }

        TalonsFS::FileSystem fs(device);

        if (!fs.format()) {
            std::cout << "Format failed\n";
            return 1;
        }

        std::cout << "Formatted: 1\n";

        if (!fs.createDirectory(directoryPath)) {
            std::cout << "Directory creation failed\n";
            return 1;
        }

        if (!fs.createFile(filePath)) {
            std::cout << "File creation failed\n";
            return 1;
        }
        
	TalonsFS::Node createdNode{};

if (!fs.getNode(filePath, createdNode)) {
    std::cout << "Failed to read created file metadata\n";
    return 1;
}

if (createdNode.createdTime == 0) {
    std::cout << "createdTime is unset\n";
    return 1;
}

if (createdNode.modifiedTime == 0) {
    std::cout << "modifiedTime is unset\n";
    return 1;
}

if (createdNode.createdTime != createdNode.modifiedTime) {
    std::cout << "Creation and modification timestamps differ\n";
    return 1;
}

    originalCreatedTime =
    createdNode.createdTime;

   const uint64_t originalModifiedTime =
    createdNode.modifiedTime;

        std::vector<uint8_t> data(
            message.begin(),
            message.end()
        );
while (TalonsFS::currentUnixTime() <= originalModifiedTime)
{
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)
    );
}
        if (!fs.writeFile(filePath, data)) {
            std::cout << "File write failed\n";
            return 1;
        }
TalonsFS::Node modifiedNode{};

if (!fs.getNode(filePath, modifiedNode)) {
    std::cout << "Failed to read modified file metadata\n";
    return 1;
}

if (modifiedNode.createdTime != originalCreatedTime) {
    std::cout << "createdTime changed after write\n";
    return 1;
}

if (modifiedNode.modifiedTime <= originalModifiedTime) {
    std::cout << "modifiedTime did not increase after write\n";
    return 1;
}

modifiedTime =
    modifiedNode.modifiedTime;

std::cout << "Timestamp update test: 1\n";      

if (!fs.unmount()) {
            std::cout << "Unmount failed\n";
            return 1;
        }

if (fs.isMounted()) {
    std::cout << "Filesystem still mounted after unmount\n";
    return 1;
}

if (fs.exists(filePath)) {
    std::cout << "Filesystem operation succeeded while unmounted\n";
    return 1;
}

std::cout << "Unmount operation guard test: 1\n";

std::cout << "Unmount state test: 1\n";

        std::cout << "First session saved: 1\n";
    }

    /*
     * Second session.
     *
     * The first FileSystem and FileBlockDevice
     * objects no longer exist.
     */

    {
        TalonsFS::FileBlockDevice device(
            imagePath,
            totalBlocks,
            blockSize
        );

        if (!device.isOpen()) {
            std::cout << "Reopen failed\n";
            return 1;
        }

        TalonsFS::FileSystem fs(device);

        if (!fs.mount()) {
            std::cout << "Mount failed\n";
            return 1;
        }

        std::cout << "Remounted: 1\n";

	if (!fs.exists(directoryPath)) {
    std::cout << "Directory missing\n";
    return 1;
}

if (!fs.exists(filePath)) {
    std::cout << "File missing\n";
    return 1;
}

TalonsFS::Node persistedNode{};

if (!fs.getNode(filePath, persistedNode)) {
    std::cout << "Failed to read persisted file metadata\n";
    return 1;
}

if (persistedNode.createdTime != originalCreatedTime) {
    std::cout << "createdTime did not persist\n";
    return 1;
}

if (persistedNode.modifiedTime != modifiedTime) {
    std::cout << "modifiedTime did not persist\n";
    return 1;
}

std::cout << "Timestamp persistence test: 1\n";

std::vector<uint8_t> data;

        if (!fs.readFile(filePath, data)) {
            std::cout << "Read failed\n";
            return 1;
        }

        std::string result(
            data.begin(),
            data.end()
        );

        std::cout << "Recovered contents: "
                  << result
                  << '\n';

        if (result != message) {
            std::cout << "Contents corrupted\n";
            return 1;
        }

        std::cout << "Persistence test: 1\n";

        if (!fs.unmount()) {
            std::cout << "Final unmount failed\n";
            return 1;
        }
    }

    return 0;
}
