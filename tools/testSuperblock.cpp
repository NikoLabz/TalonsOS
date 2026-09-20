#include <cstdio>
#include <cstdint>

#include "talonsfs/file_block_device.h"
#include "talonsfs/superblock_utils.h"
#include "block_device.h"
#include "talonsfs/constants.h"

int main()
{
    const uint64_t blockSize =
        TalonsFS::TALONSFS_BLOCK_SIZE;

    const uint64_t fsBlockCount =
        TOTAL_DISK_BLOCKS - FILESYSTEM_START;

    const uint64_t byteOffset =
        FILESYSTEM_START * blockSize;

    const char* diskPath =
        "build/disk.img";

    const char* tempPath =
        "build/testSuperblock.fs";

    std::FILE* input =
        std::fopen(diskPath, "rb");

    if (input == nullptr)
    {
        std::printf(
            "Could not open disk image\n"
        );

        return 1;
    }

    std::FILE* output =
        std::fopen(tempPath, "wb");

    if (output == nullptr)
    {
        std::fclose(input);

        std::printf(
            "Could not create test image\n"
        );

        return 1;
    }

    if (std::fseek(
            input,
            static_cast<long>(byteOffset),
            SEEK_SET
        ) != 0)
    {
        std::fclose(input);
        std::fclose(output);

        return 1;
    }

    char buffer[4096];

    uint64_t remaining =
        fsBlockCount * blockSize;

    while (remaining > 0)
    {
        const std::size_t chunk =
            remaining < sizeof(buffer)
                ? static_cast<std::size_t>(remaining)
                : sizeof(buffer);

        const std::size_t read =
            std::fread(
                buffer,
                1,
                chunk,
                input
            );

        if (read != chunk)
        {
            std::fclose(input);
            std::fclose(output);

            return 1;
        }

        if (std::fwrite(
                buffer,
                1,
                read,
                output
            ) != read)
        {
            std::fclose(input);
            std::fclose(output);

            return 1;
        }

        remaining -= read;
    }

    std::fclose(input);
    std::fclose(output);

    /*
     * Test 1:
     * The original filesystem must validate.
     */

    {
        TalonsFS::FileBlockDevice device(
            tempPath,
            fsBlockCount,
            TalonsFS::TALONSFS_BLOCK_SIZE
        );

        if (!device.isOpen())
        {
            std::remove(tempPath);

            std::printf(
                "Could not open test filesystem\n"
            );

            return 1;
        }

        TalonsFS::Superblock superblock{};

        if (!TalonsFS::readSuperblock(
                device,
                superblock))
        {
            std::printf(
                "Normal filesystem validation: FAILED\n"
            );

            std::remove(tempPath);

            return 1;
        }

        std::printf(
            "Normal filesystem validation: PASSED\n"
        );

        /*
         * Test 2:
         * Corrupt the version field.
         */

        superblock.version ^= 1;

        if (TalonsFS::validateSuperblock(
                superblock))
        {
            std::printf(
                "Corrupted filesystem validation: FAILED\n"
            );

            std::remove(tempPath);

            return 1;
        }

        std::printf(
            "Corrupted filesystem validation: PASSED\n"
        );

        /*
         * Test 3:
         * Create an impossible filesystem layout.
         */

        TalonsFS::Superblock invalidLayout =
            superblock;

        invalidLayout.version =
            1;

        invalidLayout.dataStart =
            invalidLayout.totalBlocks;

        /*
         * Recalculate the checksum so that the
         * checksum itself is valid.
         *
         * This means the following test specifically
         * tests layout validation.
         */

        invalidLayout.checksum =
            TalonsFS::calculateSuperblockChecksum(
                invalidLayout
            );

        if (TalonsFS::validateSuperblock(
                invalidLayout))
        {
            std::printf(
                "Invalid layout validation: FAILED\n"
            );

            std::remove(tempPath);

            return 1;
        }

        std::printf(
            "Invalid layout validation: PASSED\n"
        );
    }

    std::remove(tempPath);

    return 0;
}
