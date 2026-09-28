#include <cstdint>
#include <iostream>

#include "../include/talonsfs/memory_block_device.h"
#include "../../../kernel/block_cache.h"

int main()
{
    constexpr uint32_t BLOCK_SIZE = 512;
    constexpr uint64_t BLOCK_COUNT = 64;

    TalonsFS::MemoryBlockDevice device(
        BLOCK_COUNT,
        BLOCK_SIZE
    );

    BlockCache cache(device);

    uint8_t writeData[BLOCK_SIZE] = {};
    uint8_t readData[BLOCK_SIZE] = {};

    // Fill every cache entry with a different block.
    for (uint64_t block = 0;
         block < BlockCache::CACHE_BLOCK_COUNT;
         ++block)
    {
        writeData[0] = static_cast<uint8_t>(block + 1);

        if (!cache.writeBlock(block, writeData))
        {
            std::cerr
                << "FAIL: initial write failed for block "
                << block << '\n';

            return 1;
        }
    }

    // All 32 entries are now dirty in the cache.
    //
    // Writing block 32 forces entry 0 to be evicted.
    writeData[0] = 33;

    if (!cache.writeBlock(
            BlockCache::CACHE_BLOCK_COUNT,
            writeData))
    {
        std::cerr
            << "FAIL: eviction-triggering write failed\n";

        return 1;
    }

    // Block 0 should have been flushed when its cache entry
    // was replaced.
    uint8_t directData[BLOCK_SIZE] = {};

    if (!device.readBlock(0, directData))
    {
        std::cerr
            << "FAIL: direct read of evicted block failed\n";

        return 1;
    }

    if (directData[0] != 1)
    {
        std::cerr
            << "FAIL: evicted dirty block was not flushed\n";

        return 1;
    }

    // Block 32 should still be available through the cache.
    if (!cache.readBlock(
            BlockCache::CACHE_BLOCK_COUNT,
            readData))
    {
        std::cerr
            << "FAIL: read of newly cached block failed\n";

        return 1;
    }

    if (readData[0] != 33)
    {
        std::cerr
            << "FAIL: cached block 32 contains incorrect data\n";

        return 1;
    }

    // Flush everything remaining in the cache.
    if (!cache.flush())
    {
        std::cerr
            << "FAIL: final cache flush failed\n";

        return 1;
    }

    // Verify another block that remained dirty in the cache
    // eventually reached the underlying device.
    directData[0] = 0;

    if (!device.readBlock(31, directData))
    {
        std::cerr
            << "FAIL: direct read of block 31 failed\n";

        return 1;
    }

    if (directData[0] != 32)
    {
        std::cerr
            << "FAIL: final flush did not write block 31\n";

        return 1;
    }

	    // Test that invalidate() flushes dirty data before
    // discarding the cache entries.
    writeData[0] = 77;

    if (!cache.writeBlock(40, writeData))
    {
        std::cerr
            << "FAIL: invalidate test write failed\n";

        return 1;
    }

    if (!cache.invalidate())
    {
        std::cerr
            << "FAIL: invalidate failed\n";

        return 1;
    }

    directData[0] = 0;

    if (!device.readBlock(40, directData))
    {
        std::cerr
            << "FAIL: direct read after invalidate failed\n";

        return 1;
    }

    if (directData[0] != 77)
    {
        std::cerr
            << "FAIL: invalidate discarded dirty data\n";

        return 1;
    }

    std::cout
        << "PASS: safe cache invalidation\n";

    std::cout
        << "PASS: cache eviction and write-back\n";

    return 0;
}
