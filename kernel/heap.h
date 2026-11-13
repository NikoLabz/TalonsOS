#pragma once

// Kernel heap core (Phase 5, M3).
//
// A general purpose allocator over one contiguous region. Every block has
// an in-heap header; blocks are kept in address order and adjacent free
// blocks are always merged ("coalesced"). Allocation is first-fit (lowest
// address) with splitting. The implementation has no hardware or kernel
// dependencies, so it is unit-tested on the host against ordinary buffers;
// kernel/memory.cpp binds one instance to the real kernel heap region.
//
// Block layout (all addresses 16-byte aligned):
//
//   +--------------------+  32-byte header: state magic, payload size,
//   | BlockHeader        |  size of the physically previous block,
//   +--------------------+  requested size, header checksum
//   | user bytes         |  <- pointer returned to the caller
//   | slack (0xA5)       |  rounding up to a 16-byte multiple
//   | canary (8 bytes)   |  address/size dependent value
//   +--------------------+
//
// Free block payloads are filled with 0xDD ("poison"). Detected problems
// are counted, remembered and passed to an optional handler; the heap
// refuses to touch memory it cannot validate rather than "repairing" it.

#include <stddef.h>
#include <stdint.h>

namespace talons_heap
{
    constexpr size_t ALIGNMENT = 16;
    constexpr size_t HEADER_SIZE = 32;
    constexpr size_t CANARY_SIZE = 8;
    constexpr size_t MIN_PAYLOAD = 16;
    constexpr size_t MIN_BLOCK = HEADER_SIZE + MIN_PAYLOAD;

    enum class Error : uint32_t
    {
        None = 0,
        OutOfMemory,       // no free block large enough
        DoubleFree,        // pointer to an already freed block
        InvalidFree,       // pointer that is not a live allocation
        HeaderCorruption,  // a block header failed validation
        Overflow,          // canary or slack of a block was overwritten
        UseAfterFree       // poison of a free block was overwritten
    };

    // Called after the heap state is consistent. The handler must NOT call
    // back into the allocator.
    typedef void (*ErrorHandler)(Error error, const void* address,
                                 size_t size);

    struct BlockHeader;

    struct Stats
    {
        // Layout / usage (computed by walking the heap).
        size_t heapSize;             // managed bytes
        size_t blockCount;
        size_t allocatedBlocks;
        size_t freeBlocks;
        size_t freeBytes;            // sum of free block payloads
        size_t largestFreeBlock;     // payload of the largest free block
        size_t fragmentationPercent; // 0 = one free block, 100 = hopeless

        // Usage counters (maintained incrementally).
        size_t bytesInUse;           // sum of requested sizes, live blocks
        size_t blockBytesInUse;      // sum of payload sizes, live blocks
        size_t peakBytesInUse;
        size_t peakBlockBytesInUse;
        uint64_t totalAllocations;   // successful allocate() calls
        uint64_t totalFrees;         // successful free() calls

        // Failure / corruption counters.
        uint64_t outOfMemoryEvents;
        size_t lastFailedRequest;
        uint64_t doubleFrees;
        uint64_t invalidFrees;
        uint64_t headerCorruptions;
        uint64_t overflowsDetected;
        uint64_t useAfterFreeDetected;
        Error lastError;
        const void* lastErrorAddress;
    };

    struct CheckReport
    {
        bool ok;                // nothing wrong found
        bool truncated;         // walk stopped at an unreadable header
        size_t blocks;
        size_t allocatedBlocks;
        size_t freeBlocks;
        size_t badHeaders;
        size_t badCanaries;     // allocated blocks with canary/slack damage
        size_t badPoison;       // free blocks whose poison was disturbed
        size_t adjacentFree;    // two neighbouring free blocks (not merged)
        const void* firstProblem;
        Error firstProblemKind;
    };

    // No constructor and no default member initialisers on purpose: an
    // instance in .bss is valid (uninitialised) before init() runs.
    class Heap
    {
    public:
        // 'base' must be 16-byte aligned. 'size' is rounded down to a
        // multiple of 16 and must be at least MIN_BLOCK. The region is
        // overwritten. Returns false (heap unusable) on bad arguments.
        bool init(void* base, size_t size);
        bool isInitialised() const;

        // Returns nullptr for size 0, for failure, and when uninitialised.
        void* allocate(size_t size);

        // Safe on any pointer: nullptr is ignored; pointers that are not
        // live allocations are counted and ignored.
        void free(void* pointer);

        // Largest single request an empty heap can satisfy.
        size_t maxAllocation() const;

        // Walks the heap. Returns false if the walk hit a bad header (the
        // fields gathered up to that point are still filled in).
        bool getStats(Stats& out) const;

        // Full read-only integrity check. Returns report.ok.
        bool check(CheckReport& out) const;

        void setErrorHandler(ErrorHandler handler);

    private:
        BlockHeader* first() const;
        BlockHeader* nextOf(const BlockHeader* block) const;
        bool validHeader(const BlockHeader* block) const;
        void store(BlockHeader* block) const;
        bool overrun(const BlockHeader* block) const;
        bool insideFreeBlock(const void* address) const;
        void report(Error error, const void* address, size_t size);
        void* fail(size_t size);

        uint8_t* base_;
        uint8_t* end_;
        size_t size_;
        size_t maxAllocation_;
        bool initialised_;
        bool inHandler_;
        ErrorHandler handler_;

        size_t bytesInUse_;
        size_t blockBytesInUse_;
        size_t peakBytesInUse_;
        size_t peakBlockBytesInUse_;
        uint64_t totalAllocations_;
        uint64_t totalFrees_;
        uint64_t outOfMemoryEvents_;
        size_t lastFailedRequest_;
        uint64_t doubleFrees_;
        uint64_t invalidFrees_;
        uint64_t headerCorruptions_;
        uint64_t overflowsDetected_;
        uint64_t useAfterFreeDetected_;
        Error lastError_;
        const void* lastErrorAddress_;
    };
}
