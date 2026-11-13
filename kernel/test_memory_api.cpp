// Phase 5 M3: tests for the kmalloc()/kfree() wrapper additions in
// memory.cpp (lazy initialisation, statistics, integrity check, error
// and fatal handlers). Built with -DTALONS_HOST_TEST so the heap lives in
// a static buffer instead of physical address 0x100000.

#include "memory.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    int errorCount = 0;
    talons_heap::Error lastSeen = talons_heap::Error::None;

    void onError(talons_heap::Error error, const void*, std::size_t)
    {
        ++errorCount;
        lastSeen = error;
    }

    std::size_t fatalCalls = 0;
    std::size_t fatalSize = 0;

    void onFatal(std::size_t requested)
    {
        ++fatalCalls;
        fatalSize = requested;
    }
}

int main()
{
    using namespace talons_heap;

    Stats unavailableStats{};
    CheckReport unavailableReport{};
    assert(!memory_get_stats(unavailableStats));
    assert(!memory_check(unavailableReport));
    assert(unavailableReport.truncated);
    // Repeated reads remain unavailable: diagnostics did not lazy-initialize
    // or poison the heap before its first allocation.
    assert(!memory_get_stats(unavailableStats));

    // 1. The heap works before memory_init() has ever been called (as it
    //    did before the redesign, when the allocator state was all zero).
    void* early = kmalloc(100);
    assert(early != nullptr);
    assert(reinterpret_cast<std::uintptr_t>(early) % 16 == 0);
    std::memset(early, 0x5A, 100);
    kfree(early);

    // 2. Statistics through the public API.
    memory_init();

    Stats stats;
    assert(memory_get_stats(stats));
    assert(stats.heapSize == 0x100000);
    assert(stats.blockCount == 1 && stats.freeBlocks == 1);
    assert(stats.allocatedBlocks == 0);
    assert(stats.largestFreeBlock == stats.freeBytes);
    assert(stats.freeBytes == 0x100000 - HEADER_SIZE);
    assert(stats.fragmentationPercent == 0);
    assert(memory_max_allocation() == 0x100000 - HEADER_SIZE - CANARY_SIZE);

    void* a = kmalloc(100);
    void* b = kmalloc(200);
    assert(a && b);
    assert(memory_get_stats(stats));
    assert(stats.allocatedBlocks == 2);
    assert(stats.bytesInUse == 300);
    assert(stats.totalAllocations == 2 && stats.totalFrees == 0);
    assert(stats.peakBytesInUse == 300);

    kfree(a);
    assert(memory_get_stats(stats));
    assert(stats.bytesInUse == 200);
    assert(stats.totalFrees == 1);
    assert(stats.peakBytesInUse == 300);       // peak is sticky
    assert(stats.freeBlocks == 2);             // hole + tail

    // 3. Integrity check through the public API.
    CheckReport report;
    assert(memory_check(report));
    assert(report.ok && report.blocks == 3);
    Stats beforeCheck{};
    assert(memory_get_stats(beforeCheck));
    assert(memory_check(report));
    Stats afterCheck{};
    assert(memory_get_stats(afterCheck));
    assert(afterCheck.totalAllocations == beforeCheck.totalAllocations);
    assert(afterCheck.totalFrees == beforeCheck.totalFrees);
    assert(afterCheck.outOfMemoryEvents == beforeCheck.outOfMemoryEvents);
    assert(afterCheck.lastError == beforeCheck.lastError);

    // 4. Error handler: double free, invalid free, overflow, OOM.
    memory_set_error_handler(onError);

    kfree(a);                                  // double free
    assert(errorCount == 1 && lastSeen == Error::DoubleFree);

    int local = 0;
    kfree(&local);                             // not a heap pointer
    assert(errorCount == 2 && lastSeen == Error::InvalidFree);

    static_cast<unsigned char*>(b)[200] = 0x00;    // slack byte overwritten
    assert(!memory_check(report));
    assert(report.badCanaries == 1);
    kfree(b);
    assert(errorCount == 3 && lastSeen == Error::Overflow);

    assert(kmalloc(memory_max_allocation() + 1) == nullptr);
    assert(errorCount == 4 && lastSeen == Error::OutOfMemory);

    assert(memory_get_stats(stats));
    assert(stats.doubleFrees == 1 && stats.invalidFrees == 1);
    assert(stats.overflowsDetected == 1);
    assert(stats.outOfMemoryEvents == 1);
    assert(stats.lastFailedRequest == memory_max_allocation() + 1);

    memory_set_error_handler(nullptr);
    memory_init();
    kfree(&local);                             // no handler: still harmless
    assert(errorCount == 4);

    for (std::size_t i = 0; i < 1000; ++i)
    {
        const std::size_t size = i % 127 + 1;
        void* block = kmalloc(size);
        assert(block != nullptr);
        std::memset(block, static_cast<int>(i & 0xFF), size);
        kfree(block);
    }
    assert(memory_check(report));
    assert(memory_get_stats(stats));
    assert(stats.allocatedBlocks == 0 && stats.totalAllocations == 1000 &&
           stats.totalFrees == 1000);

    // 5. Fatal-OOM hook used by operator new.
    memory_set_fatal_handler(onFatal);
    memory_fatal_out_of_memory(1234);
    assert(fatalCalls == 1 && fatalSize == 1234);
    memory_set_fatal_handler(nullptr);

    std::puts("Memory API tests passed.");
    return 0;
}
