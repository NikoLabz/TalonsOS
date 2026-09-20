#pragma once

#include <cstdint>
#include <fstream>
#include <string>

#include "block_device.h"

namespace TalonsFS {

class FileBlockDevice : public BlockDevice {
public:
    FileBlockDevice(
        const std::string& path,
        uint64_t blockCount,
        uint32_t blockSize
    );

    ~FileBlockDevice() override;

    bool isOpen() const;

    bool readBlock(
        uint64_t blockNumber,
        void* buffer
    ) override;

    bool writeBlock(
        uint64_t blockNumber,
        const void* buffer
    ) override;

    uint64_t getBlockCount() const override;

    uint32_t getBlockSize() const override;

private:
    bool createImage();

    std::string path_;
    uint64_t blockCount_;
    uint32_t blockSize_;
    std::fstream file_;
};

}
