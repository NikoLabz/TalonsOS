#include "block_cache.h"

#include <stdint.h>

namespace
{
    constexpr uint32_t CACHE_BLOCK_SIZE = 512;
}


// ============================================================
// BlockCache
// ============================================================

BlockCache::BlockCache(
    TalonsFS::BlockDevice& device
)
    : device(device),
      nextReplacement(0),
      usable(device.getBlockSize() == CACHE_BLOCK_SIZE)
{
    /*
     * CacheEntry::data is a fixed 512 bytes. If the underlying device
     * uses a different block size, every transfer would either
     * overflow that array (device block > 512) or silently deliver a
     * partly-stale buffer to the caller (device block < 512), because
     * getBlockSize() reports the DEVICE's size while the copy loops
     * move exactly CACHE_BLOCK_SIZE bytes. Refuse to operate instead.
     */
    for (uint32_t i = 0;
         i < CACHE_BLOCK_COUNT;
         ++i)
    {
        entries[i].blockNumber = 0;
        entries[i].valid = false;
        entries[i].dirty = false;

        for (uint32_t j = 0;
             j < CACHE_BLOCK_SIZE;
             ++j)
        {
            entries[i].data[j] = 0;
        }
    }
}


BlockCache::CacheEntry* BlockCache::findEntry(
    uint64_t blockNumber
)
{
    for (uint32_t i = 0;
         i < CACHE_BLOCK_COUNT;
         ++i)
    {
        if (entries[i].valid &&
            entries[i].blockNumber == blockNumber)
        {
            return &entries[i];
        }
    }

    return nullptr;
}


BlockCache::CacheEntry* BlockCache::selectEntry()
{
    /*
     * Prefer a slot that has never been used. Plain round-robin will
     * happily evict a valid (possibly dirty) entry while free slots
     * sit unused, which costs a needless write-back and throws away a
     * cached block for nothing. Once the cache is full this falls
     * through to the original round-robin behaviour.
     */
    for (uint32_t i = 0;
         i < CACHE_BLOCK_COUNT;
         ++i)
    {
        if (!entries[i].valid)
        {
            return &entries[i];
        }
    }

    CacheEntry& entry =
        entries[nextReplacement];

    nextReplacement =
        (nextReplacement + 1) %
        CACHE_BLOCK_COUNT;

    return &entry;
}


bool BlockCache::flushEntry(
    CacheEntry& entry
)
{
    if (!entry.valid || !entry.dirty)
    {
        return true;
    }

    if (!device.writeBlock(
            entry.blockNumber,
            entry.data))
    {
        return false;
    }

    entry.dirty = false;

    return true;
}


bool BlockCache::readBlock(
    uint64_t blockNumber,
    void* buffer
)
{
    if (!usable)
    {
        return false;
    }

    if (buffer == nullptr)
    {
        return false;
    }

    if (blockNumber >= getBlockCount())
    {
        return false;
    }

    CacheEntry* entry =
        findEntry(blockNumber);

    if (entry != nullptr)
    {
        for (uint32_t i = 0;
             i < CACHE_BLOCK_SIZE;
             ++i)
        {
            reinterpret_cast<uint8_t*>(buffer)[i] =
                entry->data[i];
        }

        return true;
    }

    entry = selectEntry();

    if (!flushEntry(*entry))
    {
        return false;
    }

    if (!device.readBlock(
            blockNumber,
            entry->data))
    {
        entry->valid = false;
        entry->dirty = false;

        return false;
    }

    entry->blockNumber = blockNumber;
    entry->valid = true;
    entry->dirty = false;

    for (uint32_t i = 0;
         i < CACHE_BLOCK_SIZE;
         ++i)
    {
        reinterpret_cast<uint8_t*>(buffer)[i] =
            entry->data[i];
    }

    return true;
}


bool BlockCache::writeBlock(
    uint64_t blockNumber,
    const void* buffer
)
{
    if (!usable)
    {
        return false;
    }

    if (buffer == nullptr)
    {
        return false;
    }

    if (blockNumber >= getBlockCount())
    {
        return false;
    }

    CacheEntry* entry =
        findEntry(blockNumber);

    if (entry == nullptr)
    {
        entry = selectEntry();

        if (!flushEntry(*entry))
        {
            return false;
        }

        entry->blockNumber = blockNumber;
        entry->valid = true;
        entry->dirty = false;
    }

    const uint8_t* source =
        reinterpret_cast<const uint8_t*>(buffer);

    for (uint32_t i = 0;
         i < CACHE_BLOCK_SIZE;
         ++i)
    {
        entry->data[i] = source[i];
    }

    entry->dirty = true;

    return true;
}


uint64_t BlockCache::getBlockCount() const
{
    return device.getBlockCount();
}


uint32_t BlockCache::getBlockSize() const
{
    return device.getBlockSize();
}


bool BlockCache::isUsable() const
{
    return usable;
}


bool BlockCache::flush()
{
    for (uint32_t i = 0;
         i < CACHE_BLOCK_COUNT;
         ++i)
    {
        if (!flushEntry(entries[i]))
        {
            return false;
        }
    }

    return true;
}

bool BlockCache::invalidate()
{
    if (!flush())
    {
        return false;
    }

    for (uint32_t i = 0;
         i < CACHE_BLOCK_COUNT;
         ++i)
    {
        entries[i].valid = false;
        entries[i].dirty = false;
    }

    nextReplacement = 0;

    return true;
}
