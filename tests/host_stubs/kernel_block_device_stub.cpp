// Host-only test double for kernel/block_device.cpp.
//
// Implements the exact same KernelBlockDevice interface (declared in
// kernel/block_device.h, UNMODIFIED) but backed by a plain file
// instead of raw ATA port I/O, so kernel/filesystem.cpp -- completely
// unmodified -- can be exercised on the host for regression testing.
//
// This file lives outside kernel/ and is never linked into the real
// kernel or QEMU build; it only replaces block_device.cpp for
// host-side regression tests.

#include "block_device.h"

#include <cstdio>
#include <cstdlib>

namespace {
    std::FILE* diskFile = nullptr;
}

void test_open_disk(const char* path)
{
    diskFile = std::fopen(path, "r+b");

    if (diskFile == nullptr) {
        std::fprintf(stderr, "test harness: failed to open %s\n", path);
        std::exit(1);
    }
}

void test_close_disk()
{
    if (diskFile != nullptr) {
        std::fclose(diskFile);
        diskFile = nullptr;
    }
}

bool KernelBlockDevice::readBlock(
    uint64_t block,
    void* buffer
)
{
    if (buffer == nullptr || diskFile == nullptr) {
        return false;
    }

    if (block >= getBlockCount()) {
        return false;
    }

    const uint64_t physicalBlock = FILESYSTEM_START + block;

    if (physicalBlock >= TOTAL_DISK_BLOCKS) {
        return false;
    }

    const long offset =
        static_cast<long>(physicalBlock) * 512L;

    if (std::fseek(diskFile, offset, SEEK_SET) != 0) {
        return false;
    }

    return std::fread(buffer, 1, 512, diskFile) == 512;
}

bool KernelBlockDevice::writeBlock(
    uint64_t block,
    const void* buffer
)
{
    if (buffer == nullptr || diskFile == nullptr) {
        return false;
    }

    if (block >= getBlockCount()) {
        return false;
    }

    const uint64_t physicalBlock = FILESYSTEM_START + block;

    if (physicalBlock >= TOTAL_DISK_BLOCKS) {
        return false;
    }

    const long offset =
        static_cast<long>(physicalBlock) * 512L;

    if (std::fseek(diskFile, offset, SEEK_SET) != 0) {
        return false;
    }

    const bool ok = std::fwrite(buffer, 1, 512, diskFile) == 512;
    std::fflush(diskFile);
    return ok;
}

uint64_t KernelBlockDevice::getBlockCount() const
{
    return TOTAL_DISK_BLOCKS - FILESYSTEM_START;
}

uint32_t KernelBlockDevice::getBlockSize() const
{
    return 512;
}
