// Phase 5 M3: unit tests for the heap core (kernel/heap.cpp), run on the
// host against ordinary buffers. Covers layout, splitting, merging, the
// canary / poison / header protections, double and invalid frees, OOM,
// fragmentation and a randomised stress test with data-integrity checks.

#include "heap.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

using namespace talons_heap;

namespace
{
    constexpr size_t HEAP_BYTES = 1 << 20;
    alignas(16) unsigned char buffer[HEAP_BYTES + 64];

    int errorCalls = 0;
    Error errorKind = Error::None;
    const void* errorAddress = nullptr;
    size_t errorSize = 0;

    void onError(Error kind, const void* address, size_t size)
    {
        ++errorCalls;
        errorKind = kind;
        errorAddress = address;
        errorSize = size;
    }

    void resetErrors()
    {
        errorCalls = 0;
        errorKind = Error::None;
        errorAddress = nullptr;
        errorSize = 0;
    }

    // Stats even when the heap walk hit corruption (fields up to that
    // point, plus all counters, are valid).
    Stats rawStats(const Heap& h)
    {
        Stats s;
        h.getStats(s);
        return s;
    }

    Stats statsOf(const Heap& h)
    {
        Stats s;
        const bool ok = h.getStats(s);
        assert(ok);
        return s;
    }

    CheckReport checkOf(const Heap& h)
    {
        CheckReport r;
        h.check(r);
        return r;
    }

    bool isAligned(const void* p)
    {
        return reinterpret_cast<uintptr_t>(p) % 16 == 0;
    }

    // Layout invariant that must hold at all times:
    //   headers + free payload + allocated payload == heap size
    void assertLayout(const Heap& h)
    {
        const Stats s = statsOf(h);
        assert(s.blockCount == s.allocatedBlocks + s.freeBlocks);
        assert(s.blockCount * HEADER_SIZE + s.freeBytes +
               s.blockBytesInUse == s.heapSize);
    }

    unsigned char* headerOf(void* payload)
    {
        return static_cast<unsigned char*>(payload) - HEADER_SIZE;
    }

    // Fresh heap of 'size' bytes in the shared buffer.
    void fresh(Heap& h, size_t size = HEAP_BYTES)
    {
        const bool ok = h.init(buffer, size);
        assert(ok);
        h.setErrorHandler(onError);
        resetErrors();
    }

    // ------------------------------------------------------------------

    void testInit()
    {
        // Like the kernel's instance, this one lives in zeroed static
        // storage: an uninitialised heap must be inert.
        static Heap h;
        assert(!h.isInitialised());
        assert(h.allocate(16) == nullptr);
        h.free(buffer);
        assert(h.maxAllocation() == 0);
        Stats s;
        assert(!h.getStats(s));
        CheckReport r;
        assert(!h.check(r));

        // Bad arguments.
        assert(!h.init(nullptr, 4096));
        assert(!h.init(buffer + 8, 4096));              // misaligned
        assert(!h.init(buffer, 0));
        assert(!h.init(buffer, MIN_BLOCK - ALIGNMENT)); // too small
        assert(!h.init(buffer, size_t(1) << 33));       // too large
        assert(!h.isInitialised());

        // Smallest legal heap: one block with a 16-byte payload.
        assert(h.init(buffer, MIN_BLOCK));
        assert(h.maxAllocation() == MIN_PAYLOAD - CANARY_SIZE);
        void* p = h.allocate(8);
        assert(p == buffer + HEADER_SIZE);
        assert(h.allocate(1) == nullptr);               // full
        h.free(p);
        assert(h.allocate(8) == p);
        assert(checkOf(h).ok);

        // Size is rounded down to a multiple of 16.
        assert(h.init(buffer, 4096 + 15));
        assert(statsOf(h).heapSize == 4096);
        assert(statsOf(h).freeBytes == 4096 - HEADER_SIZE);

        // Whole region starts poisoned except the first header.
        for (size_t i = HEADER_SIZE; i < 4096; ++i)
            assert(buffer[i] == 0xDD);

        // Re-init discards everything, keeps the error handler.
        fresh(h, 8192);
        void* q = h.allocate(100);
        assert(q != nullptr);
        h.free(q);
        h.free(q);
        assert(errorCalls == 1);
        assert(h.init(buffer, 8192));
        assert(statsOf(h).totalAllocations == 0);
        assert(statsOf(h).doubleFrees == 0);
        h.free(buffer + 4096 + 16);                      // invalid
        assert(errorCalls == 2);                         // handler survived
    }

    void testSizesAndAlignment()
    {
        Heap h;
        fresh(h);

        assert(h.allocate(0) == nullptr);
        assert(errorCalls == 0);                         // not an error

        std::vector<std::pair<unsigned char*, size_t>> live;

        for (size_t size = 1; size <= 300; ++size)
        {
            unsigned char* p = static_cast<unsigned char*>(h.allocate(size));
            assert(p != nullptr);
            assert(isAligned(p));
            std::memset(p, static_cast<int>(size & 0xFF), size);
            live.push_back({p, size});
        }

        for (size_t size : {size_t(1000), size_t(4095), size_t(4096),
                            size_t(4097), size_t(65536), size_t(100000)})
        {
            unsigned char* p = static_cast<unsigned char*>(h.allocate(size));
            assert(p != nullptr && isAligned(p));
            std::memset(p, static_cast<int>(size & 0xFF), size);
            live.push_back({p, size});
        }

        // Writing exactly the requested bytes never disturbs anything.
        assert(checkOf(h).ok);
        assertLayout(h);
        assert(errorCalls == 0);

        // Distinct, non-overlapping, contents intact.
        for (size_t i = 0; i < live.size(); ++i)
        {
            for (size_t j = 0; j < live[i].second; ++j)
                assert(live[i].first[j] == (live[i].second & 0xFF));

            if (i + 1 < live.size())
                assert(live[i].first + live[i].second <= live[i + 1].first);
        }

        for (auto& entry : live)
            h.free(entry.first);

        assert(errorCalls == 0);
        const Stats s = statsOf(h);
        assert(s.blockCount == 1 && s.freeBlocks == 1);
        assert(s.freeBytes == HEAP_BYTES - HEADER_SIZE);
        assert(s.bytesInUse == 0 && s.blockBytesInUse == 0);
        assert(checkOf(h).ok);
    }

    void testSplitAndReuse()
    {
        Heap h;
        fresh(h);

        void* a = h.allocate(100);
        void* b = h.allocate(100);
        assert(a == buffer + HEADER_SIZE);               // lowest address
        assert(b > a);

        Stats s = statsOf(h);
        assert(s.allocatedBlocks == 2 && s.freeBlocks == 1);

        // A freed hole is reused first (first fit, lowest address) and
        // split when it is larger than needed.
        h.free(a);
        s = statsOf(h);
        assert(s.freeBlocks == 2);

        void* c = h.allocate(40);
        assert(c == a);
        s = statsOf(h);
        assert(s.allocatedBlocks == 2);                  // c and b
        assert(s.freeBlocks == 2);                       // remainder + tail

        // A request that does not fit the hole skips it.
        void* d = h.allocate(200);
        assert(d > b);
        assert(checkOf(h).ok);
        assertLayout(h);

        // A hole that is only slightly bigger than the request is used
        // whole (no sliver block below MIN_BLOCK is created).
        fresh(h);
        void* x = h.allocate(100);
        void* guard = h.allocate(8);
        assert(x && guard);
        h.free(x);
        const size_t before = statsOf(h).blockCount;
        void* y = h.allocate(100 - 16);                  // leaves < MIN_BLOCK
        assert(y == x);
        assert(statsOf(h).blockCount == before);
        assert(checkOf(h).ok);
    }

    void testMergeAllOrders()
    {
        int order[4] = {0, 1, 2, 3};

        do
        {
            Heap h;
            fresh(h);

            void* blocks[4];
            for (int i = 0; i < 4; ++i)
            {
                blocks[i] = h.allocate(100);
                assert(blocks[i] != nullptr);
            }

            bool freed[5] = {false, false, false, false, true};  // tail free

            for (int step = 0; step < 4; ++step)
            {
                const int victim = order[step];
                h.free(blocks[victim]);
                freed[victim] = true;

                // Expected number of free blocks = maximal runs of freed
                // blocks (the tail counts and merges with a freed D).
                size_t runs = 0;
                for (int i = 0; i < 5; ++i)
                    if (freed[i] && (i == 0 || !freed[i - 1]))
                        ++runs;

                const Stats s = statsOf(h);
                assert(s.freeBlocks == runs);
                assert(s.allocatedBlocks == static_cast<size_t>(3 - step));
                assert(checkOf(h).ok);
                assertLayout(h);
            }

            const Stats s = statsOf(h);
            assert(s.blockCount == 1);
            assert(s.freeBytes == HEAP_BYTES - HEADER_SIZE);
            assert(errorCalls == 0);

            // Fully merged: a maximum-size allocation works again.
            assert(h.allocate(h.maxAllocation()) == buffer + HEADER_SIZE);
        }
        while (std::next_permutation(order, order + 4));
    }

    void testMaxAllocationAndOom()
    {
        Heap h;
        fresh(h, 65536);

        const size_t maxReq = h.maxAllocation();
        assert(maxReq == 65536 - HEADER_SIZE - CANARY_SIZE);

        assert(h.allocate(maxReq + 1) == nullptr);
        assert(errorCalls == 1 && errorKind == Error::OutOfMemory);
        assert(errorSize == maxReq + 1);
        assert(h.allocate(SIZE_MAX) == nullptr);         // overflow-safe
        assert(h.allocate(SIZE_MAX - 8) == nullptr);
        assert(errorCalls == 3);

        void* all = h.allocate(maxReq);
        assert(all == buffer + HEADER_SIZE);
        assert(h.allocate(1) == nullptr);                // full
        assert(errorCalls == 4);

        Stats s = statsOf(h);
        assert(s.outOfMemoryEvents == 4);
        assert(s.lastFailedRequest == 1);
        assert(s.lastError == Error::OutOfMemory);
        assert(s.freeBlocks == 0 && s.freeBytes == 0);
        assert(s.fragmentationPercent == 0);             // nothing free

        std::memset(all, 0xAB, maxReq);                  // full use is legal
        assert(checkOf(h).ok);

        // A failed allocation leaves the heap untouched.
        for (size_t i = 0; i < maxReq; ++i)
            assert(static_cast<unsigned char*>(all)[i] == 0xAB);

        h.free(all);
        assert(statsOf(h).freeBlocks == 1);
        assert(h.allocate(maxReq) == all);               // recovered
        assert(checkOf(h).ok);
    }

    void testFragmentation()
    {
        Heap h;
        fresh(h, 262144);

        std::vector<void*> blocks;
        for (;;)
        {
            void* p = h.allocate(64);                    // 96+... per block
            if (p == nullptr)
                break;
            blocks.push_back(p);
        }

        assert(blocks.size() > 1000);
        assert(statsOf(h).outOfMemoryEvents == 1);

        // Free every other block: lots of free memory, no big hole.
        for (size_t i = 0; i < blocks.size(); i += 2)
            h.free(blocks[i]);

        Stats s = statsOf(h);
        assert(s.freeBlocks >= blocks.size() / 2);
        assert(s.freeBytes > 30000);
        assert(s.largestFreeBlock < 200);
        assert(s.fragmentationPercent > 90);
        assertLayout(h);

        // Enough free memory in total, but no run big enough: OOM, and
        // the statistics make the cause visible.
        const uint64_t oomBefore = s.outOfMemoryEvents;
        const size_t request = s.freeBytes / 2;
        assert(h.allocate(request) == nullptr);
        s = statsOf(h);
        assert(s.outOfMemoryEvents == oomBefore + 1);
        assert(s.lastFailedRequest == request);

        // Small requests still succeed from the holes.
        void* small = h.allocate(32);
        assert(small != nullptr);
        h.free(small);

        // Free the rest: everything merges and the big request works.
        for (size_t i = 1; i < blocks.size(); i += 2)
            h.free(blocks[i]);

        s = statsOf(h);
        assert(s.blockCount == 1 && s.fragmentationPercent == 0);
        assert(h.allocate(h.maxAllocation()) != nullptr);
    }

    void testNoAllocationLimit()
    {
        Heap h;
        fresh(h);

        std::vector<void*> blocks;
        for (int i = 0; i < 15000; ++i)
        {
            void* p = h.allocate(16);
            assert(p != nullptr);                        // old limit: 256
            blocks.push_back(p);
        }

        assert(statsOf(h).allocatedBlocks == 15000);
        assert(checkOf(h).ok);

        for (void* p : blocks)
            h.free(p);

        assert(statsOf(h).blockCount == 1);
        assert(errorCalls == 0);
    }

    void testDoubleFree()
    {
        Heap h;
        fresh(h);

        void* a = h.allocate(100);
        void* b = h.allocate(100);
        void* c = h.allocate(100);
        assert(a && b && c);

        // Case 1: block B freed (neighbours allocated, header stays).
        h.free(b);
        const Stats before = statsOf(h);
        h.free(b);
        assert(errorCalls == 1 && errorKind == Error::DoubleFree);
        assert(errorAddress == b);
        Stats after = statsOf(h);
        assert(after.doubleFrees == 1);
        assert(after.freeBlocks == before.freeBlocks);
        assert(after.totalFrees == before.totalFrees);   // not counted
        assert(checkOf(h).ok);

        // Case 2: block A freed and MERGED into B's hole (its header is
        // now poison): still recognised as a double free.
        h.free(a);
        assert(errorCalls == 1);
        h.free(a);
        assert(errorCalls == 2 && errorKind == Error::DoubleFree);
        assert(statsOf(h).doubleFrees == 2);
        assert(checkOf(h).ok);

        // The surviving allocation is unaffected.
        std::memset(c, 0x77, 100);
        assert(checkOf(h).ok);
        h.free(c);
        assert(statsOf(h).blockCount == 1);
        assert(statsOf(h).invalidFrees == 0);
    }

    void testInvalidFree()
    {
        Heap h;
        fresh(h);

        void* big = h.allocate(1000);
        void* other = h.allocate(100);
        assert(big && other);

        const Stats before = statsOf(h);
        int stackValue = 0;
        unsigned char* p = static_cast<unsigned char*>(big);

        h.free(nullptr);                                 // ignored silently
        assert(errorCalls == 0);

        const void* bad[] = {
            &stackValue,                                 // outside the heap
            buffer,                                      // heap start
            buffer + 16,                                 // inside 1st header
            p + 1,                                       // misaligned
            p + 16,                                      // inside payload
            p + 512,                                     // inside payload
            p - 16,                                      // inside header
            p - HEADER_SIZE,                             // the header itself
            buffer + HEAP_BYTES,                         // one past the end
            buffer + HEAP_BYTES + 16,                    // beyond
        };

        int expected = 0;
        for (const void* pointer : bad)
        {
            h.free(const_cast<void*>(pointer));
            ++expected;
            assert(errorCalls == expected);
            assert(errorKind == Error::InvalidFree);     // never "double"
        }

        const Stats after = statsOf(h);
        assert(after.invalidFrees == 10 && after.doubleFrees == 0);
        assert(after.totalFrees == before.totalFrees);
        assert(after.allocatedBlocks == before.allocatedBlocks);
        assert(after.bytesInUse == before.bytesInUse);
        assert(checkOf(h).ok);

        // Real frees still work afterwards.
        h.free(big);
        h.free(other);
        assert(statsOf(h).blockCount == 1);
    }

    void testOverflowDetection()
    {
        // Byte just past the request (inside the slack).
        {
            Heap h;
            fresh(h);
            unsigned char* p = static_cast<unsigned char*>(h.allocate(100));
            p[100] = 0x00;
            CheckReport r = checkOf(h);
            assert(!r.ok && r.badCanaries == 1);
            assert(r.firstProblemKind == Error::Overflow);
            h.free(p);
            assert(errorCalls == 1 && errorKind == Error::Overflow);
            assert(statsOf(h).overflowsDetected == 1);
            assert(statsOf(h).blockCount == 1);          // still freed
            assert(checkOf(h).ok);                       // poison restored
        }

        // Request that is an exact multiple of 16 has a whole 16 bytes of
        // slack+canary: writing one byte past it hits slack.
        {
            Heap h;
            fresh(h);
            unsigned char* p = static_cast<unsigned char*>(h.allocate(64));
            p[64] = 0x00;
            assert(!checkOf(h).ok);
            h.free(p);
            assert(errorKind == Error::Overflow);
        }

        // Overwrite the canary itself, byte by byte.
        for (size_t offset = 0; offset < CANARY_SIZE; ++offset)
        {
            Heap h;
            fresh(h);
            unsigned char* p = static_cast<unsigned char*>(h.allocate(100));
            // Block payload is 112: canary occupies bytes 104..111.
            p[104 + offset] ^= 0xFF;
            assert(!checkOf(h).ok);
            h.free(p);
            assert(errorCalls == 1 && errorKind == Error::Overflow);
        }

        // Overflow big enough to wreck the NEXT block's header: the free
        // of the overflowing block is refused (leaked) rather than
        // trusting a corrupt neighbour, and the corruption is reported.
        {
            Heap h;
            fresh(h);
            unsigned char* a = static_cast<unsigned char*>(h.allocate(100));
            unsigned char* b = static_cast<unsigned char*>(h.allocate(100));
            // a's block is 112 bytes; 148 bytes run 36 bytes past it and
            // destroy b's 32-byte header.
            std::memset(a, 0x41, 148);
            assert(!checkOf(h).ok);
            h.free(a);
            assert(errorCalls == 1 && errorKind == Error::HeaderCorruption);
            assert(rawStats(h).headerCorruptions == 1);
            assert(rawStats(h).totalFrees == 0);         // refused, leaked
            (void)b;
        }

        // A write BEFORE the payload (underflow) damages this block's own
        // header.
        {
            Heap h;
            fresh(h);
            void* x = h.allocate(100);
            void* a = h.allocate(100);
            unsigned char* b = static_cast<unsigned char*>(h.allocate(100));
            assert(x && a);
            b[-1] = 0x00;
            CheckReport r = checkOf(h);
            assert(!r.ok && r.badHeaders == 1 && r.truncated);

            // Allocation walks the heap and must refuse to continue.
            assert(h.allocate(8) == nullptr);
            assert(rawStats(h).headerCorruptions == 1);
            assert(errorKind == Error::HeaderCorruption);

            // A block whose NEIGHBOUR is damaged is refused (the heap
            // cannot tell whether that neighbour is free and needs
            // merging), but blocks further away are unaffected.
            h.free(a);
            assert(rawStats(h).totalFrees == 0);
            assert(rawStats(h).headerCorruptions == 2);
            h.free(x);
            assert(rawStats(h).totalFrees == 1);
            assert(rawStats(h).headerCorruptions == 2);
        }
    }

    void testUseAfterFree()
    {
        Heap h;
        fresh(h);

        unsigned char* a = static_cast<unsigned char*>(h.allocate(100));
        void* guard = h.allocate(100);
        assert(a && guard);

        h.free(a);
        for (size_t i = 0; i < 100; ++i)
            assert(a[i] == 0xDD);                        // poisoned

        assert(checkOf(h).ok);

        a[10] = 0x00;                                    // write after free
        CheckReport r = checkOf(h);
        assert(!r.ok && r.badPoison == 1);
        assert(r.firstProblemKind == Error::UseAfterFree);

        // Reallocating that block notices.
        void* again = h.allocate(100);
        assert(again == a);                              // still handed out
        assert(errorKind == Error::UseAfterFree);
        assert(statsOf(h).useAfterFreeDetected == 1);

        // The block is usable and the heap is healthy again.
        std::memset(again, 0x33, 100);
        assert(checkOf(h).ok);

        // A stray write beyond the part being handed out is not seen by
        // allocate(), but the full check finds it.
        h.free(again);
        h.free(guard);
        unsigned char* c = static_cast<unsigned char*>(h.allocate(32));
        assert(c != nullptr);
        resetErrors();
        unsigned char* tail = c + 4000;                  // inside the tail
        tail[0] = 0x00;
        assert(!checkOf(h).ok);
        assert(errorCalls == 0);
    }

    void testEveryHeaderByteIsProtected()
    {
        // Flip each byte of an allocated header, then of a free header:
        // the integrity check must notice every time.
        for (int state = 0; state < 2; ++state)
        {
            for (size_t byte = 0; byte < HEADER_SIZE; ++byte)
            {
                Heap h;
                fresh(h);

                void* a = h.allocate(100);
                void* b = h.allocate(100);
                void* c = h.allocate(100);
                assert(a && b && c);

                void* target = b;
                if (state == 1)
                {
                    h.free(b);                           // b becomes free
                    target = b;
                }

                unsigned char* header = headerOf(target);
                header[byte] ^= 0xFF;

                CheckReport r = checkOf(h);
                assert(!r.ok);
                assert(r.badHeaders >= 1);
                assert(r.firstProblemKind == Error::HeaderCorruption);

                header[byte] ^= 0xFF;                    // restore
                assert(checkOf(h).ok);
            }
        }
    }

    void testBackLinkProtected()
    {
        // prevSize of a block does not match the previous block.
        Heap h;
        fresh(h);
        h.allocate(100);
        void* b = h.allocate(100);
        h.allocate(100);

        // Forge a *valid-looking* header for b with a wrong prevSize by
        // recomputing nothing: any single-field change breaks the
        // checksum, so also verify the neighbour-consistency check by
        // freeing a block whose successor was damaged.
        unsigned char* next = headerOf(b) + HEADER_SIZE + 112;
        next[20] ^= 0x01;                                // c's prevSize
        assert(!checkOf(h).ok);

        h.free(b);                                       // must refuse
        assert(errorKind == Error::HeaderCorruption);
        assert(rawStats(h).headerCorruptions == 1);
        assert(rawStats(h).totalFrees == 0);
    }

    // Gaps found by mutation testing: every neighbour check must bite.
    void testNeighbourValidation()
    {
        // Free is refused when the PREVIOUS block's header is damaged.
        {
            Heap h;
            fresh(h);
            void* a = h.allocate(100);
            void* b = h.allocate(100);
            void* c = h.allocate(100);
            assert(a && b && c);

            headerOf(a)[9] ^= 0xFF;                      // damage a
            h.free(b);                                   // b's prev is a
            assert(errorCalls == 1 && errorKind == Error::HeaderCorruption);
            assert(rawStats(h).totalFrees == 0);
            headerOf(a)[9] ^= 0xFF;
            assert(checkOf(h).ok);
            h.free(b);                                   // fine once repaired
            assert(statsOf(h).totalFrees == 1);
        }

        // Allocation that would split a block is refused when the block
        // AFTER it is damaged (it must not rewrite an unvalidated header).
        {
            Heap h;
            fresh(h);
            void* x = h.allocate(100);
            void* y = h.allocate(100);
            void* z = h.allocate(100);
            assert(x && y && z);
            h.free(y);                                   // 112-byte hole

            unsigned char* zHeader = headerOf(z);
            zHeader[26] ^= 0xFF;                         // damage z

            assert(h.allocate(40) == nullptr);           // would split y
            assert(errorKind == Error::HeaderCorruption);
            assert(rawStats(h).headerCorruptions == 1);
            zHeader[26] ^= 0xFF;
            assert(h.allocate(40) == y);                 // fine once repaired
            assert(checkOf(h).ok);
        }

        // A header that is self-consistent (valid checksum) but carries
        // the wrong back-link is still caught. Block headers of equal
        // blocks differ only in prevSize, so copying the first block's
        // header over the second forges exactly that.
        {
            Heap h;
            fresh(h);
            void* x = h.allocate(100);
            void* y = h.allocate(100);
            void* z = h.allocate(100);
            assert(x && y && z);

            std::memcpy(headerOf(y), headerOf(x), HEADER_SIZE);
            CheckReport r = checkOf(h);
            assert(!r.ok && r.badHeaders == 1 && r.truncated);

            assert(h.allocate(8) == nullptr);            // refuses to walk on
            assert(errorKind == Error::HeaderCorruption);
        }

        // Two neighbouring free blocks (merge invariant broken) are
        // reported even though every header is valid and poison intact.
        {
            Heap h;
            fresh(h);
            void* x = h.allocate(100);
            void* y = h.allocate(100);
            void* z = h.allocate(100);
            assert(x && y && z);
            h.free(y);                                   // y: free header

            std::memset(z, 0xDD, 112);                   // z payload = poison
            std::memcpy(headerOf(z), headerOf(y), HEADER_SIZE);

            CheckReport r = checkOf(h);
            assert(!r.ok);
            assert(r.adjacentFree == 2);                 // y-z and z-tail
            assert(r.badPoison == 0 && r.badHeaders == 0);
        }
    }

    void testStatsAndPeaks()
    {
        Heap h;
        fresh(h);

        void* a = h.allocate(100);
        void* b = h.allocate(250);
        Stats s = statsOf(h);
        assert(s.bytesInUse == 350);
        assert(s.blockBytesInUse == 112 + 272);
        assert(s.peakBytesInUse == 350 && s.peakBlockBytesInUse == 384);
        assert(s.totalAllocations == 2);

        h.free(a);
        s = statsOf(h);
        assert(s.bytesInUse == 250 && s.blockBytesInUse == 272);
        assert(s.peakBytesInUse == 350);                 // peak sticks
        assert(s.totalFrees == 1);

        h.free(b);
        s = statsOf(h);
        assert(s.bytesInUse == 0 && s.blockBytesInUse == 0);
        assert(s.peakBytesInUse == 350);
        assert(s.totalAllocations == 2 && s.totalFrees == 2);
        assert(s.lastError == Error::None);
        assertLayout(h);
    }

    void testErrorHandlerIsGuarded()
    {
        // Handler that (wrongly) triggers another error: no recursion.
        static Heap* target;
        static int calls;
        static void* victim;

        Heap h;
        fresh(h);
        target = &h;
        calls = 0;
        victim = h.allocate(32);
        h.free(victim);

        h.setErrorHandler([](Error, const void*, size_t)
        {
            ++calls;
            target->free(victim);                        // another double free
        });

        h.free(victim);                                  // triggers handler
        assert(calls == 1);                              // not re-entered
        assert(statsOf(h).doubleFrees == 2);             // both counted

        h.setErrorHandler(nullptr);
        h.free(victim);                                  // no handler: fine
        assert(calls == 1);
    }

    // ------------------------------------------------------------------
    // Randomised stress test with data-integrity checking.

    struct Rng
    {
        uint64_t s;
        uint64_t next()
        {
            s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s;
        }
        size_t below(size_t n) { return static_cast<size_t>(next() % n); }
    };

    struct Live
    {
        unsigned char* pointer;
        size_t size;
        unsigned char pattern;
    };

    void testRandomised(uint64_t seed)
    {
        Heap h;
        fresh(h, 262144);
        Rng rng{seed};

        std::vector<Live> live;
        std::map<uintptr_t, size_t> intervals;           // start -> size

        auto verifyContents = [&](const Live& l)
        {
            for (size_t i = 0; i < l.size; ++i)
                assert(l.pointer[i] == l.pattern);
        };

        for (int step = 0; step < 20000; ++step)
        {
            const bool doAllocate = live.empty() || rng.below(100) < 55;

            if (doAllocate)
            {
                size_t size;
                switch (rng.below(10))
                {
                case 0:  size = 1 + rng.below(8000); break;
                case 1:  size = 1 + rng.below(1000); break;
                default: size = 1 + rng.below(120);  break;
                }

                unsigned char* p =
                    static_cast<unsigned char*>(h.allocate(size));

                if (p == nullptr)
                {
                    assert(errorKind == Error::OutOfMemory);
                    // Make room: free a few random blocks.
                    for (int k = 0; k < 5 && !live.empty(); ++k)
                    {
                        const size_t i = rng.below(live.size());
                        verifyContents(live[i]);
                        intervals.erase(
                            reinterpret_cast<uintptr_t>(live[i].pointer));
                        h.free(live[i].pointer);
                        live[i] = live.back();
                        live.pop_back();
                    }
                    continue;
                }

                assert(isAligned(p));
                const uintptr_t start = reinterpret_cast<uintptr_t>(p);

                // No overlap with any live allocation.
                auto it = intervals.lower_bound(start);
                if (it != intervals.end())
                    assert(start + size <= it->first);
                if (it != intervals.begin())
                {
                    --it;
                    assert(it->first + it->second <= start);
                }

                intervals[start] = size;
                const unsigned char pattern =
                    static_cast<unsigned char>(1 + rng.below(250));
                std::memset(p, pattern, size);
                live.push_back({p, size, pattern});
            }
            else
            {
                const size_t i = rng.below(live.size());
                verifyContents(live[i]);
                intervals.erase(reinterpret_cast<uintptr_t>(live[i].pointer));
                h.free(live[i].pointer);
                live[i] = live.back();
                live.pop_back();
            }

            if (step % 97 == 0)
            {
                const CheckReport r = checkOf(h);
                assert(r.ok);
                assertLayout(h);

                size_t sum = 0;
                for (const Live& l : live)
                    sum += l.size;
                assert(statsOf(h).bytesInUse == sum);
                assert(statsOf(h).allocatedBlocks == live.size());
            }
        }

        // No error other than OutOfMemory ever fired.
        assert(statsOf(h).doubleFrees == 0);
        assert(statsOf(h).invalidFrees == 0);
        assert(statsOf(h).overflowsDetected == 0);
        assert(statsOf(h).useAfterFreeDetected == 0);
        assert(statsOf(h).headerCorruptions == 0);

        // Free everything in random order: back to a single free block.
        while (!live.empty())
        {
            const size_t i = rng.below(live.size());
            verifyContents(live[i]);
            h.free(live[i].pointer);
            live[i] = live.back();
            live.pop_back();
        }

        const Stats s = statsOf(h);
        assert(s.blockCount == 1 && s.freeBlocks == 1);
        assert(s.freeBytes == 262144 - HEADER_SIZE);
        assert(s.bytesInUse == 0);
        assert(checkOf(h).ok);
    }
}

int main()
{
    testInit();
    testSizesAndAlignment();
    testSplitAndReuse();
    testMergeAllOrders();
    testMaxAllocationAndOom();
    testFragmentation();
    testNoAllocationLimit();
    testDoubleFree();
    testInvalidFree();
    testOverflowDetection();
    testUseAfterFree();
    testEveryHeaderByteIsProtected();
    testBackLinkProtected();
    testNeighbourValidation();
    testStatsAndPeaks();
    testErrorHandlerIsGuarded();

    for (uint64_t seed = 1; seed <= 6; ++seed)
        testRandomised(seed * 0x9E3779B97F4A7C15ULL);

    std::puts("Kernel heap tests passed.");
    return 0;
}
