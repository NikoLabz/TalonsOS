// ============================================================
// Comprehensive BlockCache audit tests.
//
// The original test_block_cache.cpp covers the happy path for
// eviction, flush and invalidate. This file covers the parts that
// are hard or impossible to provoke against real ATA hardware:
//
//   * device read failure / device write failure
//   * failure during eviction write-back
//   * failure during flush() and invalidate()
//   * block-size mismatch (fail-closed guard)
//   * bounds and null-buffer rejection
//   * dirty-state and overwrite semantics
//   * free-slot-preferred replacement
//
// Uses a fault-injecting mock BlockDevice so every error path is
// deterministic.
// ============================================================

#include "../include/talonsfs/block_device.h"
#include "../../../kernel/block_cache.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
    int failures = 0;

    void check(bool condition, const char* what)
    {
        std::printf("%s: %s\n", what, condition ? "PASSED" : "FAILED");
        if (!condition)
        {
            ++failures;
        }
    }

    // A block device that can be told to fail specific operations.
    class FaultyDevice : public TalonsFS::BlockDevice
    {
    public:
        FaultyDevice(uint64_t count, uint32_t size)
            : blockCount_(count),
              blockSize_(size),
              storage_(count * size, 0)
        {
        }

        bool readBlock(uint64_t block, void* buffer) override
        {
            ++reads;

            if (failReads || block == failReadBlock)
            {
                return false;
            }

            if (block >= blockCount_)
            {
                return false;
            }

            std::memcpy(buffer,
                        storage_.data() + block * blockSize_,
                        blockSize_);
            return true;
        }

        bool writeBlock(uint64_t block, const void* buffer) override
        {
            ++writes;

            if (failWrites || block == failWriteBlock)
            {
                return false;
            }

            if (block >= blockCount_)
            {
                return false;
            }

            std::memcpy(storage_.data() + block * blockSize_,
                        buffer,
                        blockSize_);
            return true;
        }

        uint64_t getBlockCount() const override { return blockCount_; }
        uint32_t getBlockSize()  const override { return blockSize_; }

        uint8_t peek(uint64_t block) const
        {
            return storage_[block * blockSize_];
        }

        void poke(uint64_t block, uint8_t value)
        {
            std::memset(storage_.data() + block * blockSize_,
                        value, blockSize_);
        }

        bool failReads  = false;
        bool failWrites = false;
        uint64_t failReadBlock  = UINT64_MAX;
        uint64_t failWriteBlock = UINT64_MAX;

        long reads  = 0;
        long writes = 0;

    private:
        uint64_t blockCount_;
        uint32_t blockSize_;
        std::vector<uint8_t> storage_;
    };

    void fill(uint8_t* buf, uint8_t value)
    {
        std::memset(buf, value, 512);
    }
}

int main()
{
    // =========================================================
    // Argument validation
    // =========================================================
    {
        FaultyDevice dev(64, 512);
        BlockCache cache(dev);
        uint8_t buf[512] = {};

        check(cache.isUsable(),
              "512-byte device is accepted");

        check(!cache.readBlock(0, nullptr),
              "readBlock rejects a null buffer");

        check(!cache.writeBlock(0, nullptr),
              "writeBlock rejects a null buffer");

        check(!cache.readBlock(64, buf),
              "readBlock rejects an out-of-range block");

        check(!cache.writeBlock(64, buf),
              "writeBlock rejects an out-of-range block");

        check(!cache.readBlock(UINT64_MAX, buf),
              "readBlock rejects UINT64_MAX without overflowing");
    }

    // =========================================================
    // Block-size mismatch must fail closed, not corrupt memory
    // =========================================================
    {
        FaultyDevice dev(16, 1024);
        BlockCache cache(dev);
        uint8_t buf[1024] = {};

        check(!cache.isUsable(),
              "1024-byte device is refused");

        check(!cache.readBlock(0, buf),
              "readBlock fails closed on block-size mismatch");

        check(!cache.writeBlock(0, buf),
              "writeBlock fails closed on block-size mismatch");
    }

    // =========================================================
    // Cache hit avoids the device
    // =========================================================
    {
        FaultyDevice dev(64, 512);
        BlockCache cache(dev);
        uint8_t buf[512] = {};

        dev.poke(7, 0xA5);

        cache.readBlock(7, buf);
        const long afterFirst = dev.reads;

        cache.readBlock(7, buf);

        check(dev.reads == afterFirst,
              "Second read of the same block is a cache hit");

        check(buf[0] == 0xA5,
              "Cache hit returns the correct data");
    }

    // =========================================================
    // Write-back: a write does not immediately reach the device
    // =========================================================
    {
        FaultyDevice dev(64, 512);
        BlockCache cache(dev);
        uint8_t buf[512];

        fill(buf, 0x3C);
        cache.writeBlock(5, buf);

        check(dev.peek(5) == 0x00,
              "Write is buffered, not written through");

        check(cache.flush(),
              "flush() succeeds");

        check(dev.peek(5) == 0x3C,
              "flush() pushes dirty data to the device");
    }

    // =========================================================
    // Overwriting a cached block keeps one entry and stays dirty
    // =========================================================
    {
        FaultyDevice dev(64, 512);
        BlockCache cache(dev);
        uint8_t buf[512];

        fill(buf, 0x11);
        cache.writeBlock(9, buf);

        fill(buf, 0x22);
        cache.writeBlock(9, buf);

        const long writesBefore = dev.writes;
        cache.flush();

        check(dev.writes == writesBefore + 1,
              "Repeated writes to one block coalesce into one device write");

        check(dev.peek(9) == 0x22,
              "Last write wins after coalescing");
    }

    // =========================================================
    // Read-after-write consistency through the cache
    // =========================================================
    {
        FaultyDevice dev(64, 512);
        BlockCache cache(dev);
        uint8_t wbuf[512];
        uint8_t rbuf[512] = {};

        fill(wbuf, 0x77);
        cache.writeBlock(12, wbuf);
        cache.readBlock(12, rbuf);

        check(rbuf[0] == 0x77,
              "Read after write returns the buffered data");
    }

    // =========================================================
    // Eviction flushes the dirty victim
    // =========================================================
    {
        FaultyDevice dev(128, 512);
        BlockCache cache(dev);
        uint8_t buf[512];

        for (uint64_t b = 0; b < BlockCache::CACHE_BLOCK_COUNT; ++b)
        {
            fill(buf, static_cast<uint8_t>(b + 1));
            cache.writeBlock(b, buf);
        }

        fill(buf, 0xFF);
        cache.writeBlock(BlockCache::CACHE_BLOCK_COUNT, buf);

        check(dev.peek(0) == 1,
              "Evicted dirty entry was written back");

        check(dev.peek(1) == 0,
              "Only the victim entry was evicted");
    }

    // =========================================================
    // Free slots are used before any valid entry is evicted
    // =========================================================
    {
        FaultyDevice dev(128, 512);
        BlockCache cache(dev);
        uint8_t buf[512];

        // Fill exactly CACHE_BLOCK_COUNT entries; no write-back should
        // happen yet because every slot was free.
        for (uint64_t b = 0; b < BlockCache::CACHE_BLOCK_COUNT; ++b)
        {
            fill(buf, static_cast<uint8_t>(b + 1));
            cache.writeBlock(b, buf);
        }

        check(dev.writes == 0,
              "Filling free slots causes no write-back");
    }

    // =========================================================
    // Device READ failure must propagate, not fake success
    // =========================================================
    {
        FaultyDevice dev(64, 512);
        BlockCache cache(dev);
        uint8_t buf[512] = {};

        dev.failReadBlock = 3;

        check(!cache.readBlock(3, buf),
              "Device read failure propagates to the caller");

        // The failed block must not be left cached as valid.
        dev.failReadBlock = UINT64_MAX;
        dev.poke(3, 0x6B);

        check(cache.readBlock(3, buf) && buf[0] == 0x6B,
              "Failed read left no poisoned entry behind");
    }

    // =========================================================
    // Device WRITE failure during flush must propagate
    // =========================================================
    {
        FaultyDevice dev(64, 512);
        BlockCache cache(dev);
        uint8_t buf[512];

        fill(buf, 0x44);
        cache.writeBlock(8, buf);

        dev.failWrites = true;

        check(!cache.flush(),
              "flush() reports failure when the device write fails");
    }

    // =========================================================
    // Failed flush must NOT let invalidate() discard dirty data
    // =========================================================
    {
        FaultyDevice dev(64, 512);
        BlockCache cache(dev);
        uint8_t buf[512];

        fill(buf, 0x55);
        cache.writeBlock(10, buf);

        dev.failWrites = true;

        check(!cache.invalidate(),
              "invalidate() fails when the dirty flush fails");

        // Recover the device; the data must still be in the cache.
        dev.failWrites = false;

        check(cache.flush(),
              "Retry flush succeeds after the device recovers");

        check(dev.peek(10) == 0x55,
              "Dirty data survived the failed invalidate()");
    }

    // =========================================================
    // Successful invalidate() drops entries but keeps data on disk
    // =========================================================
    {
        FaultyDevice dev(64, 512);
        BlockCache cache(dev);
        uint8_t buf[512];

        fill(buf, 0x66);
        cache.writeBlock(11, buf);

        check(cache.invalidate(),
              "invalidate() succeeds on a healthy device");

        check(dev.peek(11) == 0x66,
              "invalidate() flushed dirty data first");

        // After invalidation the next read must go to the device:
        // change the device behind the cache and confirm it is seen.
        dev.poke(11, 0x99);
        const long before = dev.reads;
        cache.readBlock(11, buf);

        check(dev.reads == before + 1 && buf[0] == 0x99,
              "invalidate() really dropped the cached entry");
    }

    // =========================================================
    // Eviction write-back failure must propagate
    // =========================================================
    {
        FaultyDevice dev(128, 512);
        BlockCache cache(dev);
        uint8_t buf[512];

        for (uint64_t b = 0; b < BlockCache::CACHE_BLOCK_COUNT; ++b)
        {
            fill(buf, static_cast<uint8_t>(b + 1));
            cache.writeBlock(b, buf);
        }

        // Block 0 is the round-robin victim; make its write-back fail.
        dev.failWriteBlock = 0;

        fill(buf, 0xEE);

        check(!cache.writeBlock(BlockCache::CACHE_BLOCK_COUNT, buf),
              "Write fails when the evicted victim cannot be flushed");
    }

    // =========================================================
    // getBlockCount / getBlockSize delegate to the device
    // =========================================================
    {
        FaultyDevice dev(4096, 512);
        BlockCache cache(dev);

        check(cache.getBlockCount() == 4096,
              "getBlockCount() delegates to the device");

        check(cache.getBlockSize() == 512,
              "getBlockSize() delegates to the device");
    }

    if (failures != 0)
    {
        std::printf("\n%d cache test(s) FAILED\n", failures);
        return 1;
    }

    std::printf("\nAll BlockCache audit tests passed\n");
    return 0;
}
