#pragma once

#include <stdint.h>

#include "talonsfs/block_device.h"

// ============================================================
// DISK LAYOUT CONSTANTS (shared by block_device.cpp and
// filesystem.cpp -- defined ONCE here so the two files can
// never disagree about where TalonsFS starts on disk).
// ============================================================
//
// Sector 0            : bootloader
// Sectors 1..127      : kernel.bin (budget: 127 sectors = 65024 bytes)
// Sector 128 onward    : TalonsFS (first FILESYSTEM_START blocks
//                        are physical/absolute; everything TalonsFS
//                        itself deals with is FILESYSTEM-RELATIVE,
//                        i.e. block 0 as seen by TalonsFS is physical
//                        block FILESYSTEM_START).
//
// NOTE: The Makefile enforces (at build time) that kernel.bin does
// not exceed the reserved budget below, and tools/mkTalonsFS is
// invoked with this exact same offset. If you ever change this
// value, update both places.

constexpr uint64_t FILESYSTEM_START   = 128;
constexpr uint64_t TOTAL_DISK_BLOCKS  = 32768;

// KernelBlockDevice implements the TalonsFS::BlockDevice interface
// so the real TalonsFS building blocks (NodeTable, Bitmap,
// BlockAllocator, NodeAllocator, NodeManager, NodeDataManager,
// DataBlockManager, ...) can talk to the physical disk through the
// same abstraction the host-side tests and tools use.
//
// All block numbers passed into readBlock()/writeBlock() are
// FILESYSTEM-RELATIVE. The physical-block translation
// (+ FILESYSTEM_START) happens once, right here, and nowhere else.
class KernelBlockDevice : public TalonsFS::BlockDevice
{
public:

    bool readBlock(
        uint64_t block,
        void* buffer
    ) override;

    bool writeBlock(
        uint64_t block,
        const void* buffer
    ) override;

    uint64_t getBlockCount() const override;

    uint32_t getBlockSize() const override;
};
