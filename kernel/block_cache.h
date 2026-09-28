#pragma once

#include <stdint.h>

#include "talonsfs/block_device.h"

class BlockCache : public TalonsFS::BlockDevice
{
public:
    static constexpr uint32_t CACHE_BLOCK_COUNT = 32;

    explicit BlockCache(
        TalonsFS::BlockDevice& device
    );

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

    bool flush();

    bool invalidate();

    /*
     * False when the cache refused to attach to the device -- currently
     * only when the device's block size is not CACHE_BLOCK_SIZE. Every
     * read and write fails while this is false, rather than silently
     * truncating transfers and overflowing CacheEntry::data.
     */
    bool isUsable() const;

private:
    struct CacheEntry
    {
        uint64_t blockNumber;
        bool valid;
        bool dirty;
        uint8_t data[512];
    };

    TalonsFS::BlockDevice& device;

    CacheEntry entries[CACHE_BLOCK_COUNT];

    uint32_t nextReplacement;

    bool usable;

    CacheEntry* findEntry(
        uint64_t blockNumber
    );

    CacheEntry* selectEntry();

    bool flushEntry(
        CacheEntry& entry
    );
};
