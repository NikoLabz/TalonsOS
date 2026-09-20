// tools/mkTalonsFS.cpp
//
// Host-side tool (NOT freestanding — normal g++, uses <fstream> etc.).
//
// Formats a fresh TalonsFS filesystem and writes it into an existing
// disk image at the exact byte offset the kernel expects.
//
// Why this exists / what it fixes:
//
// KernelBlockDevice (kernel/block_device.cpp) treats block 0 of the
// filesystem as PHYSICAL block FILESYSTEM_START (128), not physical
// block 0 of the raw disk:
//
//     physicalBlock = FILESYSTEM_START + block;
//
// TalonsFS::FileBlockDevice has no concept of an offset — it always
// maps its block 0 to byte 0 of whatever file it's given, and it
// requires that file to be exactly blockCount * blockSize bytes.
//
// So formatting directly against disk.img with FileBlockDevice would
// write the superblock at physical block 0/1 of the disk image —
// which is exactly what corrupts the boot sector/kernel region and
// is NOT where the kernel looks for it. The kernel reads
// filesystem-relative block 1, which translates to PHYSICAL block
// 128 + 1 = 129.
//
// This tool formats into a temporary filesystem-only image sized
// exactly (TOTAL_DISK_BLOCKS - FILESYSTEM_START) blocks, then splices
// those bytes into disk.img starting at byte
// FILESYSTEM_START * BLOCK_SIZE, leaving the boot sector and kernel
// (bytes before that offset) untouched.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "talonsfs/file_block_device.h"
#include "talonsfs/filesystem.h"
#include "talonsfs/formatter.h"
#include "talonsfs/constants.h"

// Shared with the kernel — see kernel/block_device.h. Included here so
// the tool and the kernel can never disagree about where TalonsFS
// starts on disk; there is exactly one place these constants live.
#include "block_device.h"

namespace {

bool ensureMinimumSize(
    const std::string& path,
    uint64_t minimumBytes
)
{
    std::fstream file(
        path,
        std::ios::in | std::ios::out | std::ios::binary
    );

    if (!file)
    {
        // Doesn't exist yet — create it zero-filled at the full size.
        std::ofstream create(
            path,
            std::ios::binary | std::ios::trunc
        );

        if (!create)
        {
            return false;
        }

        create.seekp(
            static_cast<std::streamoff>(minimumBytes - 1)
        );
        create.put('\0');
        return static_cast<bool>(create);
    }

    file.seekg(0, std::ios::end);
    const uint64_t currentSize =
        static_cast<uint64_t>(file.tellg());

    if (currentSize >= minimumBytes)
    {
        return true;
    }

    file.seekp(
        static_cast<std::streamoff>(minimumBytes - 1)
    );
    file.put('\0');
    return static_cast<bool>(file);
}

bool spliceIntoDisk(
    const std::string& fsImagePath,
    const std::string& diskImagePath,
    uint64_t byteOffset,
    uint64_t byteCount
)
{
    std::ifstream fsImage(
        fsImagePath,
        std::ios::binary
    );

    if (!fsImage)
    {
        std::fprintf(
            stderr,
            "mkTalonsFS: could not reopen formatted image\n"
        );
        return false;
    }

    std::fstream disk(
        diskImagePath,
        std::ios::in | std::ios::out | std::ios::binary
    );

    if (!disk)
    {
        std::fprintf(
            stderr,
            "mkTalonsFS: could not open %s\n",
            diskImagePath.c_str()
        );
        return false;
    }

    disk.seekp(
        static_cast<std::streamoff>(byteOffset)
    );

    constexpr std::size_t ChunkSize = 1 << 16;
    char buffer[ChunkSize];

    uint64_t remaining = byteCount;

    while (remaining > 0)
    {
        const std::size_t toRead =
            remaining < ChunkSize
                ? static_cast<std::size_t>(remaining)
                : ChunkSize;

        fsImage.read(buffer, static_cast<std::streamsize>(toRead));

        if (!fsImage)
        {
            std::fprintf(
                stderr,
                "mkTalonsFS: short read while splicing\n"
            );
            return false;
        }

        disk.write(buffer, static_cast<std::streamsize>(toRead));

        if (!disk)
        {
            std::fprintf(
                stderr,
                "mkTalonsFS: short write while splicing\n"
            );
            return false;
        }

        remaining -= toRead;
    }

    disk.flush();
    return static_cast<bool>(disk);
}

}

int main(int argc, char** argv)
{
    const std::string diskImagePath =
        (argc > 1) ? argv[1] : "build/disk.img";

    const uint64_t totalDiskBytes =
        TOTAL_DISK_BLOCKS *
        static_cast<uint64_t>(TalonsFS::TALONSFS_BLOCK_SIZE);

    if (!ensureMinimumSize(diskImagePath, totalDiskBytes))
    {
        std::fprintf(
            stderr,
            "mkTalonsFS: failed to prepare %s\n",
            diskImagePath.c_str()
        );
        return 1;
    }

    const uint64_t fsBlockCount =
        TOTAL_DISK_BLOCKS - FILESYSTEM_START;

    const std::string tempFsPath =
        diskImagePath + ".fs.tmp";

    TalonsFS::Superblock superblock{};

    {
        TalonsFS::FileBlockDevice fsDevice(
            tempFsPath,
            fsBlockCount,
            TalonsFS::TALONSFS_BLOCK_SIZE
        );

        if (!fsDevice.isOpen())
        {
            std::fprintf(
                stderr,
                "mkTalonsFS: failed to create temporary "
                "filesystem image %s\n",
                tempFsPath.c_str()
            );
            return 1;
        }

TalonsFS::FileSystem filesystem(fsDevice);

if (!filesystem.format())
{
    std::fprintf(
        stderr,
        "mkTalonsFS: FileSystem::format() failed\n"
    );
    return 1;
}

const std::vector<std::string> directories = {
    "/Laboratory",
    "/DoTheThings",
    "/PleaseConfigureThese",
    "/ThingsKeepChanging",
    "/ProbablyTemporary",
    "/HumanThings",
    "/ThingsWithWires",
    "/TheSystemIsBusy",
    "/ExtraThings",
    "/TheServerIsWorking",
    "/PutThingsHere",
    "/UnpluggedThings",
    "/WeFoundThese"
};

for (const std::string& directory : directories)
{
    if (!filesystem.createDirectory(directory))
    {
        std::fprintf(
            stderr,
            "mkTalonsFS: failed to create %s\n",
            directory.c_str()
        );
        return 1;
    }
}

superblock = filesystem.superblock();
superblock = filesystem.superblock();

        // fsDevice goes out of scope here and flushes/closes the
        // temp file before we reopen it below for splicing.
    }

    const uint64_t byteOffset =
        FILESYSTEM_START *
        static_cast<uint64_t>(TalonsFS::TALONSFS_BLOCK_SIZE);

    const uint64_t byteCount =
        fsBlockCount *
        static_cast<uint64_t>(TalonsFS::TALONSFS_BLOCK_SIZE);

    const bool spliced = spliceIntoDisk(
        tempFsPath,
        diskImagePath,
        byteOffset,
        byteCount
    );

    std::remove(tempFsPath.c_str());

    if (!spliced)
    {
        return 1;
    }

    std::printf(
        "mkTalonsFS: formatted %llu blocks, "
        "wrote into %s at byte offset %llu "
        "(physical block %llu)\n",
        static_cast<unsigned long long>(fsBlockCount),
        diskImagePath.c_str(),
        static_cast<unsigned long long>(byteOffset),
        static_cast<unsigned long long>(FILESYSTEM_START)
    );

    std::printf(
        "  magic=0x%llx version=%u blockSize=%u "
        "totalBlocks=%llu dataStart=%llu rootNode=%llu\n",
        static_cast<unsigned long long>(superblock.magic),
        superblock.version,
        superblock.blockSize,
        static_cast<unsigned long long>(superblock.totalBlocks),
        static_cast<unsigned long long>(superblock.dataStart),
        static_cast<unsigned long long>(superblock.rootNode)
    );

    return 0;
}
