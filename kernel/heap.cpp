#include "heap.h"

#include <string.h>

namespace talons_heap
{
    struct BlockHeader
    {
        uint64_t magic;      // ALLOC_MAGIC or FREE_MAGIC
        uint64_t size;       // payload bytes, multiple of 16
        uint64_t prevSize;   // payload of the previous block; 0 if first
        uint32_t requested;  // bytes asked for (allocated), 0 when free
        uint32_t check;      // checksum of the fields above
    };

    static_assert(sizeof(BlockHeader) == HEADER_SIZE,
                  "block header must be 32 bytes");

    namespace
    {
        constexpr uint64_t ALLOC_MAGIC = 0xA11C0CA7ED0B10C5ULL;
        constexpr uint64_t FREE_MAGIC = 0xF4EEB10C5D0F4EE5ULL;
        constexpr uint64_t CANARY_SEED = 0xC4A4A4A4C0DEC0DEULL;
        constexpr uint8_t POISON = 0xDD;
        constexpr uint8_t SLACK = 0xA5;

        // requested is a uint32_t, so the heap must stay below 4 GiB.
        constexpr size_t MAX_HEAP_SIZE = 0xFFFFFFF0u;

        uint32_t headerCheck(const BlockHeader& h)
        {
            uint64_t x = h.magic ^ 0x9E3779B97F4A7C15ULL;
            x ^= h.size * 0xBF58476D1CE4E5B9ULL;
            x ^= (h.prevSize + 0x94D049BB133111EBULL) *
                 0xD6E8FEB86659FD93ULL;
            x ^= static_cast<uint64_t>(h.requested) * 0xFF51AFD7ED558CCDULL;
            x ^= x >> 32;
            x *= 0xC4CEB9FE1A85EC53ULL;
            x ^= x >> 29;
            return static_cast<uint32_t>(x);
        }

        uint64_t canaryFor(const BlockHeader* h)
        {
            return CANARY_SEED ^ reinterpret_cast<uintptr_t>(h) ^
                   (h->size << 17);
        }

        uint8_t* payloadOf(const BlockHeader* h)
        {
            return const_cast<uint8_t*>(
                reinterpret_cast<const uint8_t*>(h)) + HEADER_SIZE;
        }

        bool poisonIntact(const uint8_t* p, size_t length)
        {
            for (size_t i = 0; i < length; ++i)
                if (p[i] != POISON)
                    return false;
            return true;
        }
    }

    bool Heap::init(void* base, size_t size)
    {
        initialised_ = false;
        inHandler_ = false;

        if (base == nullptr ||
            reinterpret_cast<uintptr_t>(base) % ALIGNMENT != 0)
            return false;

        size &= ~(ALIGNMENT - 1);

        if (size < MIN_BLOCK || size > MAX_HEAP_SIZE)
            return false;

        base_ = static_cast<uint8_t*>(base);
        end_ = base_ + size;
        size_ = size;

        memset(base_, POISON, size);

        BlockHeader* h = reinterpret_cast<BlockHeader*>(base_);
        h->magic = FREE_MAGIC;
        h->size = size - HEADER_SIZE;
        h->prevSize = 0;
        h->requested = 0;
        store(h);

        maxAllocation_ = static_cast<size_t>(h->size) - CANARY_SIZE;

        bytesInUse_ = 0;
        blockBytesInUse_ = 0;
        peakBytesInUse_ = 0;
        peakBlockBytesInUse_ = 0;
        totalAllocations_ = 0;
        totalFrees_ = 0;
        outOfMemoryEvents_ = 0;
        lastFailedRequest_ = 0;
        doubleFrees_ = 0;
        invalidFrees_ = 0;
        headerCorruptions_ = 0;
        overflowsDetected_ = 0;
        useAfterFreeDetected_ = 0;
        lastError_ = Error::None;
        lastErrorAddress_ = nullptr;

        // The error handler deliberately survives re-initialisation.
        initialised_ = true;
        return true;
    }

    bool Heap::isInitialised() const
    {
        return initialised_;
    }

    size_t Heap::maxAllocation() const
    {
        return initialised_ ? maxAllocation_ : 0;
    }

    void Heap::setErrorHandler(ErrorHandler handler)
    {
        handler_ = handler;
    }

    BlockHeader* Heap::first() const
    {
        return reinterpret_cast<BlockHeader*>(base_);
    }

    BlockHeader* Heap::nextOf(const BlockHeader* block) const
    {
        uint8_t* next = const_cast<uint8_t*>(
            reinterpret_cast<const uint8_t*>(block)) +
            HEADER_SIZE + block->size;

        return next >= end_ ? nullptr
                            : reinterpret_cast<BlockHeader*>(next);
    }

    void Heap::store(BlockHeader* block) const
    {
        block->check = headerCheck(*block);
    }

    bool Heap::validHeader(const BlockHeader* block) const
    {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(block);

        if (p < base_ || p >= end_ ||
            static_cast<size_t>(end_ - p) < HEADER_SIZE)
            return false;

        if ((reinterpret_cast<uintptr_t>(p) -
             reinterpret_cast<uintptr_t>(base_)) % ALIGNMENT != 0)
            return false;

        if (block->magic != ALLOC_MAGIC && block->magic != FREE_MAGIC)
            return false;

        if (block->check != headerCheck(*block))
            return false;

        if (block->size < MIN_PAYLOAD || block->size % ALIGNMENT != 0 ||
            block->size > static_cast<size_t>(end_ - p) - HEADER_SIZE)
            return false;

        if (block->magic == ALLOC_MAGIC &&
            (block->requested == 0 ||
             static_cast<uint64_t>(block->requested) + CANARY_SIZE >
                 block->size))
            return false;

        if (block->magic == FREE_MAGIC && block->requested != 0)
            return false;

        return true;
    }

    // True if the canary or the slack bytes of an allocated block were
    // modified. The header must already have been validated.
    bool Heap::overrun(const BlockHeader* block) const
    {
        const uint8_t* payload = payloadOf(block);
        const size_t canaryAt =
            static_cast<size_t>(block->size) - CANARY_SIZE;

        for (size_t i = block->requested; i < canaryAt; ++i)
            if (payload[i] != SLACK)
                return true;

        uint64_t canary;
        memcpy(&canary, payload + canaryAt, sizeof(canary));
        return canary != canaryFor(block);
    }

    // True if 'address' lies inside (header or payload of) a FREE block.
    // Error path only: walks the heap. Returns false on any bad header.
    bool Heap::insideFreeBlock(const void* address) const
    {
        const uint8_t* target = static_cast<const uint8_t*>(address);
        uint64_t expectedPrev = 0;

        for (const BlockHeader* h = first(); h != nullptr; h = nextOf(h))
        {
            if (!validHeader(h) || h->prevSize != expectedPrev)
                return false;

            const uint8_t* start = reinterpret_cast<const uint8_t*>(h);

            if (target >= start && target < start + HEADER_SIZE + h->size)
                return h->magic == FREE_MAGIC;

            expectedPrev = h->size;
        }

        return false;
    }

    void Heap::report(Error error, const void* address, size_t size)
    {
        switch (error)
        {
        case Error::OutOfMemory:
            ++outOfMemoryEvents_;
            lastFailedRequest_ = size;
            break;
        case Error::DoubleFree:        ++doubleFrees_; break;
        case Error::InvalidFree:       ++invalidFrees_; break;
        case Error::HeaderCorruption:  ++headerCorruptions_; break;
        case Error::Overflow:          ++overflowsDetected_; break;
        case Error::UseAfterFree:      ++useAfterFreeDetected_; break;
        case Error::None:              return;
        }

        lastError_ = error;
        lastErrorAddress_ = address;

        if (handler_ != nullptr && !inHandler_)
        {
            inHandler_ = true;
            handler_(error, address, size);
            inHandler_ = false;
        }
    }

    void* Heap::fail(size_t size)
    {
        report(Error::OutOfMemory, nullptr, size);
        return nullptr;
    }

    void* Heap::allocate(size_t size)
    {
        if (!initialised_ || size == 0)
            return nullptr;

        // maxAllocation_ is far below SIZE_MAX, so this also rejects
        // requests that would overflow the size arithmetic below.
        if (size > maxAllocation_)
            return fail(size);

        const size_t need =
            (size + CANARY_SIZE + ALIGNMENT - 1) & ~(ALIGNMENT - 1);

        BlockHeader* found = nullptr;
        uint64_t expectedPrev = 0;

        for (BlockHeader* h = first(); h != nullptr; h = nextOf(h))
        {
            if (!validHeader(h) || h->prevSize != expectedPrev)
            {
                report(Error::HeaderCorruption, h, 0);
                return nullptr;
            }

            if (h->magic == FREE_MAGIC && h->size >= need)
            {
                found = h;
                break;
            }

            expectedPrev = h->size;
        }

        if (found == nullptr)
            return fail(size);

        uint8_t* payload = payloadOf(found);
        const size_t total = static_cast<size_t>(found->size);

        // The block after 'found' is about to be modified (when
        // splitting); never rewrite a header that was not validated.
        BlockHeader* after = nextOf(found);

        if (after != nullptr &&
            (!validHeader(after) || after->prevSize != total))
        {
            report(Error::HeaderCorruption, after, 0);
            return nullptr;
        }

        if (!poisonIntact(payload, need))
            report(Error::UseAfterFree, payload, need);

        if (total - need >= MIN_BLOCK)
        {
            BlockHeader* rest =
                reinterpret_cast<BlockHeader*>(payload + need);

            rest->magic = FREE_MAGIC;
            rest->size = total - need - HEADER_SIZE;
            rest->prevSize = need;
            rest->requested = 0;
            store(rest);

            if (after != nullptr)
            {
                after->prevSize = rest->size;
                store(after);
            }

            found->size = need;
        }

        found->magic = ALLOC_MAGIC;
        found->requested = static_cast<uint32_t>(size);
        store(found);

        const size_t blockSize = static_cast<size_t>(found->size);
        memset(payload + size, SLACK, blockSize - CANARY_SIZE - size);

        const uint64_t canary = canaryFor(found);
        memcpy(payload + blockSize - CANARY_SIZE, &canary, sizeof(canary));

        ++totalAllocations_;
        bytesInUse_ += size;
        blockBytesInUse_ += blockSize;

        if (bytesInUse_ > peakBytesInUse_)
            peakBytesInUse_ = bytesInUse_;
        if (blockBytesInUse_ > peakBlockBytesInUse_)
            peakBlockBytesInUse_ = blockBytesInUse_;

        return payload;
    }

    void Heap::free(void* pointer)
    {
        if (!initialised_ || pointer == nullptr)
            return;

        const uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
        const uintptr_t low = reinterpret_cast<uintptr_t>(base_);
        const uintptr_t high = reinterpret_cast<uintptr_t>(end_);

        if (address < low + HEADER_SIZE || address >= high ||
            (address - low) % ALIGNMENT != 0)
        {
            report(Error::InvalidFree, pointer, 0);
            return;
        }

        BlockHeader* h = reinterpret_cast<BlockHeader*>(
            static_cast<uint8_t*>(pointer) - HEADER_SIZE);

        if (!validHeader(h))
        {
            // A block that was freed and then merged into a neighbour has
            // its old header overwritten with poison and now lies inside
            // a free block. Anything else is simply not an allocation.
            const bool stale =
                poisonIntact(reinterpret_cast<const uint8_t*>(h),
                             HEADER_SIZE) &&
                insideFreeBlock(pointer);

            report(stale ? Error::DoubleFree : Error::InvalidFree,
                   pointer, 0);
            return;
        }

        if (h->magic == FREE_MAGIC)
        {
            report(Error::DoubleFree, pointer, 0);
            return;
        }

        // ---- validate every header that will be touched ----

        BlockHeader* next = nextOf(h);

        if (next != nullptr &&
            (!validHeader(next) || next->prevSize != h->size))
        {
            report(Error::HeaderCorruption, next, 0);
            return;
        }

        if (next != nullptr && next->magic == FREE_MAGIC)
        {
            BlockHeader* afterNext = nextOf(next);

            if (afterNext != nullptr &&
                (!validHeader(afterNext) ||
                 afterNext->prevSize != next->size))
            {
                report(Error::HeaderCorruption, afterNext, 0);
                return;
            }
        }

        BlockHeader* prev = nullptr;

        if (h->prevSize == 0)
        {
            if (reinterpret_cast<uint8_t*>(h) != base_)
            {
                report(Error::HeaderCorruption, h, 0);
                return;
            }
        }
        else
        {
            const uintptr_t hAddr = reinterpret_cast<uintptr_t>(h);

            if (h->prevSize > hAddr - low - HEADER_SIZE)
            {
                report(Error::HeaderCorruption, h, 0);
                return;
            }

            prev = reinterpret_cast<BlockHeader*>(
                hAddr - HEADER_SIZE - static_cast<size_t>(h->prevSize));

            if (!validHeader(prev) || prev->size != h->prevSize)
            {
                report(Error::HeaderCorruption, prev, 0);
                return;
            }
        }

        // ---- the free is going ahead ----

        if (overrun(h))
            report(Error::Overflow, pointer, h->requested);

        const size_t requested = h->requested;
        const size_t blockSize = static_cast<size_t>(h->size);

        ++totalFrees_;
        bytesInUse_ -= requested;
        blockBytesInUse_ -= blockSize;

        h->magic = FREE_MAGIC;
        h->requested = 0;
        store(h);
        memset(payloadOf(h), POISON, blockSize);

        // Merge with the following block.
        if (next != nullptr && next->magic == FREE_MAGIC)
        {
            const size_t nextSize = static_cast<size_t>(next->size);

            memset(next, POISON, HEADER_SIZE);
            h->size += HEADER_SIZE + nextSize;
            store(h);
        }

        // Merge into the preceding block.
        if (prev != nullptr && prev->magic == FREE_MAGIC)
        {
            prev->size += HEADER_SIZE + h->size;
            memset(h, POISON, HEADER_SIZE);
            store(prev);
            h = prev;
        }

        // Fix the back-link of whatever follows the (merged) block.
        BlockHeader* following = nextOf(h);

        if (following != nullptr)
        {
            following->prevSize = h->size;
            store(following);
        }
    }

    bool Heap::getStats(Stats& out) const
    {
        out = Stats{};

        if (!initialised_)
            return false;

        out.heapSize = size_;
        out.bytesInUse = bytesInUse_;
        out.blockBytesInUse = blockBytesInUse_;
        out.peakBytesInUse = peakBytesInUse_;
        out.peakBlockBytesInUse = peakBlockBytesInUse_;
        out.totalAllocations = totalAllocations_;
        out.totalFrees = totalFrees_;
        out.outOfMemoryEvents = outOfMemoryEvents_;
        out.lastFailedRequest = lastFailedRequest_;
        out.doubleFrees = doubleFrees_;
        out.invalidFrees = invalidFrees_;
        out.headerCorruptions = headerCorruptions_;
        out.overflowsDetected = overflowsDetected_;
        out.useAfterFreeDetected = useAfterFreeDetected_;
        out.lastError = lastError_;
        out.lastErrorAddress = lastErrorAddress_;

        bool complete = true;
        uint64_t expectedPrev = 0;

        for (const BlockHeader* h = first(); h != nullptr; h = nextOf(h))
        {
            if (!validHeader(h) || h->prevSize != expectedPrev)
            {
                complete = false;
                break;
            }

            ++out.blockCount;

            if (h->magic == FREE_MAGIC)
            {
                ++out.freeBlocks;
                out.freeBytes += static_cast<size_t>(h->size);

                if (h->size > out.largestFreeBlock)
                    out.largestFreeBlock = static_cast<size_t>(h->size);
            }
            else
            {
                ++out.allocatedBlocks;
            }

            expectedPrev = h->size;
        }

        if (out.freeBytes != 0)
            out.fragmentationPercent =
                100 - (out.largestFreeBlock * 100) / out.freeBytes;

        return complete;
    }

    bool Heap::check(CheckReport& out) const
    {
        out = CheckReport{};
        out.firstProblemKind = Error::None;

        if (!initialised_)
        {
            out.truncated = true;
            return false;
        }

        auto problem = [&](Error kind, const void* where)
        {
            if (out.firstProblem == nullptr)
            {
                out.firstProblem = where;
                out.firstProblemKind = kind;
            }
        };

        uint64_t expectedPrev = 0;
        bool previousFree = false;

        for (const BlockHeader* h = first(); h != nullptr; h = nextOf(h))
        {
            if (!validHeader(h) || h->prevSize != expectedPrev)
            {
                ++out.badHeaders;
                out.truncated = true;
                problem(Error::HeaderCorruption, h);
                break;
            }

            ++out.blocks;

            if (h->magic == FREE_MAGIC)
            {
                ++out.freeBlocks;

                if (previousFree)
                {
                    ++out.adjacentFree;
                    problem(Error::HeaderCorruption, h);
                }

                if (!poisonIntact(payloadOf(h),
                                  static_cast<size_t>(h->size)))
                {
                    ++out.badPoison;
                    problem(Error::UseAfterFree, h);
                }

                previousFree = true;
            }
            else
            {
                ++out.allocatedBlocks;

                if (overrun(h))
                {
                    ++out.badCanaries;
                    problem(Error::Overflow, payloadOf(h));
                }

                previousFree = false;
            }

            expectedPrev = h->size;
        }

        out.ok = !out.truncated && out.badHeaders == 0 &&
                 out.badCanaries == 0 && out.badPoison == 0 &&
                 out.adjacentFree == 0;
        return out.ok;
    }
}
