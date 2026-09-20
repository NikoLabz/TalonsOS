// NOTE: host-only. Uses <vector>/<algorithm>/<limits> or other hosted-C++/libc
// features and cannot compile in the freestanding kernel build. Used by the
// TalonsFS test suite and host-side tools (e.g. mkTalonsFS) only — never
// #include this from kernel/ code.

#include <cstdio>
#include <filesystem>
#include <limits>

#include "../include/talonsfs/file_block_device.h"

namespace TalonsFS {

FileBlockDevice::FileBlockDevice(
    const std::string& path,
    uint64_t blockCount,
    uint32_t blockSize
)
    : path_(path),
      blockCount_(blockCount),
      blockSize_(blockSize),
      file_()
{
    if (blockCount_ == 0 || blockSize_ == 0) {
        return;
    }

    const uint64_t maximumSize =
        std::numeric_limits<uint64_t>::max();

    if (blockCount_ >
        maximumSize / blockSize_)
    {
        return;
    }

    const uint64_t expectedSize =
        blockCount_ * blockSize_;

    std::error_code error;

    if (!std::filesystem::exists(path_, error)) {
        if (!createImage()) {
            return;
        }
    }

    if (error) {
        return;
    }

    const uint64_t actualSize =
        std::filesystem::file_size(path_, error);

    if (error || actualSize != expectedSize) {
        return;
    }

    file_.open(
        path_,
        std::ios::in |
        std::ios::out |
        std::ios::binary
    );
}

FileBlockDevice::~FileBlockDevice()
{
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

bool FileBlockDevice::isOpen() const
{
    return file_.is_open();
}

bool FileBlockDevice::createImage()
{
    std::ofstream output(
        path_,
        std::ios::binary |
        std::ios::trunc
    );

    if (!output) {
        return false;
    }

    const uint64_t size =
        blockCount_ *
        static_cast<uint64_t>(blockSize_);

    if (size == 0) {
        return false;
    }

    output.seekp(
        static_cast<std::streamoff>(size - 1)
    );

    if (!output) {
        return false;
    }

    const char zero = '\0';

    output.write(
        &zero,
        1
    );

    return static_cast<bool>(output);
}

bool FileBlockDevice::readBlock(
    uint64_t blockNumber,
    void* buffer
)
{
    if (!isOpen() ||
        buffer == nullptr ||
        blockNumber >= blockCount_)
    {
        return false;
    }

    const uint64_t offset =
        blockNumber *
        static_cast<uint64_t>(blockSize_);

    file_.clear();

    file_.seekg(
        static_cast<std::streamoff>(offset),
        std::ios::beg
    );

    if (!file_) {
        return false;
    }

    file_.read(
        static_cast<char*>(buffer),
        static_cast<std::streamsize>(blockSize_)
    );

    return file_.gcount() ==
        static_cast<std::streamsize>(blockSize_);
}

bool FileBlockDevice::writeBlock(
    uint64_t blockNumber,
    const void* buffer
)
{
    if (!isOpen() ||
        buffer == nullptr ||
        blockNumber >= blockCount_)
    {
        return false;
    }

    const uint64_t offset =
        blockNumber *
        static_cast<uint64_t>(blockSize_);

    file_.clear();

    file_.seekp(
        static_cast<std::streamoff>(offset),
        std::ios::beg
    );

    if (!file_) {
        return false;
    }

    file_.write(
        static_cast<const char*>(buffer),
        static_cast<std::streamsize>(blockSize_)
    );

    if (!file_) {
        return false;
    }

    file_.flush();

    return static_cast<bool>(file_);
}

uint64_t FileBlockDevice::getBlockCount() const
{
    return blockCount_;
}

uint32_t FileBlockDevice::getBlockSize() const
{
    return blockSize_;
}

}
