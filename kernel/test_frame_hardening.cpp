// Phase 5 M2 tests for the frame allocator additions.
//
// This test #includes frame_allocator.cpp directly so it can corrupt the
// allocator's private bookkeeping and prove that verify() notices. The
// existing test_frame_allocator.cpp is unchanged and still covers the
// original API.

#include "frame_allocator.cpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace talons_frames;
using talons_e820::Entry;
using talons_e820::Header;

namespace
{
    Header headerFor(uint32_t count)
    {
        return Header{talons_e820::MAGIC, count,
                      talons_e820::ENTRY_SIZE, 0};
    }

    // xorshift64: deterministic, no <random> needed.
    struct Rng
    {
        uint64_t state;
        uint64_t next()
        {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            return state;
        }
        size_t below(size_t n) { return static_cast<size_t>(next() % n); }
    };

    // ---- 1. Frame 0 is never handed out -------------------------------
    void testFrameZeroReserved()
    {
        // Usable from address 0 and NO reserved ranges: the allocator
        // itself must still keep frame 0 away from callers.
        Entry map[] = {{0x0, 0x4000, 1, 1}};
        Header header = headerFor(1);

        assert(init(header, map, nullptr, 0));
        assert(isFree(0x1000));
        assert(!isFree(0));
        assert(!isAllocated(0));
        assert(freeFrameCount() == 3);

        assert(allocate() == 0x1000);
        assert(allocate() == 0x2000);
        assert(allocate() == 0x3000);
        assert(allocate() == 0);
        assert(!release(0));
        assert(verify());
    }

    // ---- 2. Stats and counters ----------------------------------------
    void testStats()
    {
        Stats s;
        Entry map[] = {{0x100000, 0x10000, 1, 1}};   // 16 frames
        Header header = headerFor(1);

        // Not initialised: false and zeroed.
        assert(init(header, map, nullptr, 0));
        assert(getStats(s));
        assert(s.managedFrames == 16);
        assert(s.freeFrames == 16);
        assert(s.allocatedFrames == 0);
        assert(s.largestFreeRun == 16);
        assert(s.framesAllocatedTotal == 0);
        assert(s.framesReleasedTotal == 0);
        assert(s.failedAllocations == 0);
        assert(s.rejectedReleases == 0);
        assert(s.untrackedUsableBytes == 0);

        const uint64_t a = allocate();
        const uint64_t b = allocate();
        const uint64_t c = allocate();
        assert(a == 0x100000 && b == 0x101000 && c == 0x102000);

        assert(getStats(s));
        assert(s.freeFrames == 13);
        assert(s.allocatedFrames == 3);
        assert(s.managedFrames == s.freeFrames + s.allocatedFrames);
        assert(s.framesAllocatedTotal == 3);
        assert(s.largestFreeRun == 13);

        assert(release(b));
        assert(getStats(s));
        assert(s.allocatedFrames == 2);
        assert(s.framesReleasedTotal == 1);
        assert(s.largestFreeRun == 13);        // hole of 1 < tail of 13

        // Every kind of rejected release is counted, none changes state.
        assert(!release(b));                    // double free
        assert(!release(0x103000));             // never allocated
        assert(!release(a + 1));                // unaligned
        assert(!release(0));                    // null
        assert(!release(MAX_PHYSICAL_ADDRESS)); // out of range
        assert(getStats(s));
        assert(s.rejectedReleases == 5);
        assert(s.allocatedFrames == 2);
        assert(s.framesReleasedTotal == 1);

        // Failed allocations are counted per call.
        while (allocate() != 0) {}
        assert(getStats(s));
        assert(s.failedAllocations == 1);
        assert(allocate() == 0);
        assert(allocateContiguous(2) == 0);
        assert(getStats(s));
        assert(s.failedAllocations == 3);
        assert(s.freeFrames == 0);
        assert(s.largestFreeRun == 0);
        assert(verify());

        // Re-init clears every counter.
        assert(init(header, map, nullptr, 0));
        assert(getStats(s));
        assert(s.framesAllocatedTotal == 0 && s.failedAllocations == 0 &&
               s.rejectedReleases == 0 && s.framesReleasedTotal == 0);

        // After an invalid init the stats are unavailable and zeroed.
        Header bad = header;
        bad.magic = 0;
        assert(!init(bad, map, nullptr, 0));
        s.freeFrames = 123;
        assert(!getStats(s));
        assert(s.freeFrames == 0 && s.managedFrames == 0);
        assert(!verify());
    }

    // ---- 3. Usable memory above the managed limit ---------------------
    void testUntracked()
    {
        Stats s;

        // Entry straddling the 1 GiB limit: only the part above it is
        // untracked. Frames below the limit are managed normally.
        Entry straddle[] = {
            {MAX_PHYSICAL_ADDRESS - 0x4000, 0x14000, 1, 1}
        };
        Header header = headerFor(1);
        assert(init(header, straddle, nullptr, 0));
        assert(getStats(s));
        assert(s.managedFrames == 4);
        assert(s.untrackedUsableBytes == 0x10000);

        // Entry wholly above the limit.
        Entry above[] = {
            {0x100000, 0x4000, 1, 1},
            {MAX_PHYSICAL_ADDRESS + 0x100000, 0x200000, 1, 1}
        };
        Header two = headerFor(2);
        assert(init(two, above, nullptr, 0));
        assert(getStats(s));
        assert(s.managedFrames == 4);
        assert(s.untrackedUsableBytes == 0x200000);
        assert(allocate() == 0x100000);

        // Non-usable entries above the limit are not counted.
        Entry reservedAbove[] = {
            {0x100000, 0x4000, 1, 1},
            {MAX_PHYSICAL_ADDRESS, 0x1000, 2, 1}
        };
        assert(init(two, reservedAbove, nullptr, 0));
        assert(getStats(s));
        assert(s.untrackedUsableBytes == 0);
    }

    // ---- 4. Contiguous allocation -------------------------------------
    void testContiguous()
    {
        Stats s;

        // 16 frames at 0x100000, with frame 0x104 reserved in the middle:
        //   free: 0x100-0x103 (4), reserved 0x104, free 0x105-0x10F (11)
        Entry map[] = {{0x100000, 0x10000, 1, 1}};
        Header header = headerFor(1);
        const ReservedRange hole[] = {{0x104000, 0x1000}};
        assert(init(header, map, hole, 1));
        assert(getStats(s));
        assert(s.managedFrames == 15);
        assert(s.largestFreeRun == 11);

        // Invalid sizes.
        assert(allocateContiguous(0) == 0);
        assert(getStats(s) && s.failedAllocations == 0); // not an OOM event
        assert(allocateContiguous(MAX_FRAMES + 1) == 0);
        assert(getStats(s) && s.failedAllocations == 1);

        // Lowest fit: first 4 fit below the hole.
        assert(allocateContiguous(4) == 0x100000);
        // Next 4 cannot use the hole, so they start after it.
        assert(allocateContiguous(4) == 0x105000);
        assert(allocateContiguous(1) == 0x109000);   // behaves like allocate
        assert(verify());

        // Too large for any remaining run: fails, state unchanged.
        const size_t freeBefore = freeFrameCount();
        Stats beforeFailure;
        assert(getStats(beforeFailure));
        assert(allocateContiguous(7) == 0);          // only 6 left (0x10A-0x10F)
        assert(freeFrameCount() == freeBefore);
        Stats afterFailure;
        assert(getStats(afterFailure));
        assert(afterFailure.freeFrames == beforeFailure.freeFrames);
        assert(afterFailure.framesAllocatedTotal ==
               beforeFailure.framesAllocatedTotal);
        assert(afterFailure.framesReleasedTotal ==
               beforeFailure.framesReleasedTotal);
        assert(afterFailure.failedAllocations ==
               beforeFailure.failedAllocations + 1);
        assert(verify());
        assert(allocateContiguous(6) == 0x10A000);
        assert(freeFrameCount() == 0);
        assert(verify());

        // releaseContiguous: all-or-nothing.
        assert(!releaseContiguous(0x100000, 5));     // 0x104 is not ours
        assert(freeFrameCount() == 0);
        assert(isAllocated(0x100000) && isAllocated(0x103000));
        assert(getStats(s) && s.rejectedReleases >= 1);

        assert(releaseContiguous(0x100000, 4));
        assert(freeFrameCount() == 4);
        assert(!isAllocated(0x100000) && isFree(0x103000));

        // Hint moved back: lowest fit is again 0x100000.
        assert(allocateContiguous(2) == 0x100000);
        assert(allocate() == 0x102000);
        assert(allocate() == 0x103000);
        assert(allocate() == 0);

        // Partial releases of a larger allocation are legal.
        assert(releaseContiguous(0x106000, 2));
        assert(allocateContiguous(2) == 0x106000);

        // Invalid release arguments.
        assert(!releaseContiguous(0x100000, 0));
        assert(!releaseContiguous(0, 1));
        assert(!releaseContiguous(0x100001, 1));
        assert(!releaseContiguous(MAX_PHYSICAL_ADDRESS, 1));
        assert(!releaseContiguous(MAX_PHYSICAL_ADDRESS - 0x1000, 2));
        assert(!releaseContiguous(0x100000, static_cast<size_t>(-1)));
        assert(verify());

        // Fragmentation: largestFreeRun tracks the biggest hole.
        assert(releaseContiguous(0x105000, 1));
        assert(releaseContiguous(0x108000, 2));
        assert(getStats(s));
        assert(s.largestFreeRun == 2);
        assert(allocateContiguous(3) == 0);          // no run of 3
        assert(allocateContiguous(2) == 0x108000);   // lowest 2-run is 0x108
        assert(verify());
    }

    // ---- 5. Allocation order and hint ---------------------------------
    void testOrderAndHint()
    {
        Entry map[] = {{0x100000, 0x10000, 1, 1}};
        Header header = headerFor(1);
        assert(init(header, map, nullptr, 0));

        uint64_t f[8];
        for (int i = 0; i < 8; ++i)
            f[i] = allocate();
        for (int i = 0; i < 8; ++i)
            assert(f[i] == 0x100000 + 0x1000ULL * i);

        // Release out of order: the lowest freed frame is always reused
        // first, even though the hint had advanced past it.
        assert(release(f[5]));
        assert(release(f[2]));
        assert(release(f[6]));
        assert(allocate() == f[2]);
        assert(allocate() == f[5]);
        assert(allocate() == f[6]);
        assert(allocate() == 0x108000);
        assert(verify());
    }

    // ---- 5b. Exhaustive search-hint check -----------------------------
    //
    // Small window (10 frames, one reserved hole). For EVERY one of the
    // 2^9 allocation patterns of the 9 managed frames:
    //   * build the pattern (allocate everything, then release the frames
    //     that should be free, in a pattern-dependent order so the hint is
    //     driven both up and down),
    //   * release each allocated frame in turn (a release BELOW the current
    //     hint) and check the next allocate() returns the lowest free frame,
    //   * check allocateContiguous(1..4) returns the lowest fitting run.
    // The expected answers come from a trivial reference scan.
    void testHintExhaustive()
    {
        constexpr size_t FIRST = 0x100;
        constexpr size_t N = 10;
        constexpr size_t HOLE = 4;                 // reserved frame index

        Entry map[] = {{FIRST * FRAME_SIZE, N * FRAME_SIZE, 1, 1}};
        Header header = headerFor(1);
        const ReservedRange hole[] = {{(FIRST + HOLE) * FRAME_SIZE, FRAME_SIZE}};

        size_t managed[N - 1];
        size_t m = 0;
        for (size_t i = 0; i < N; ++i)
            if (i != HOLE)
                managed[m++] = i;
        assert(m == N - 1);

        auto addr = [&](size_t idx) { return (FIRST + idx) * FRAME_SIZE; };

        for (unsigned pattern = 0; pattern < (1u << m); ++pattern)
        {
            // bit k set => managed[k] is FREE in this pattern.
            auto buildState = [&](bool ascending) {
                assert(init(header, map, hole, 1));

                uint64_t got[N] = {};
                for (size_t k = 0; k < m; ++k)
                {
                    got[managed[k]] = allocate();
                    assert(got[managed[k]] == addr(managed[k]));
                }
                assert(freeFrameCount() == 0);

                for (size_t step = 0; step < m; ++step)
                {
                    const size_t k = ascending ? step : m - 1 - step;
                    if (pattern & (1u << k))
                        assert(release(addr(managed[k])));
                }
            };

            auto isFreeInPattern = [&](size_t idx) {
                for (size_t k = 0; k < m; ++k)
                    if (managed[k] == idx)
                        return (pattern & (1u << k)) != 0;
                return false;                       // the hole
            };

            for (int order = 0; order < 2; ++order)
            {
                buildState(order == 0);

                // Contiguous queries do not change state when they fail,
                // so check every count first, then mutate.
                for (size_t count = 1; count <= 4; ++count)
                {
                    size_t expected = N;
                    for (size_t i = 0; i + count <= N && expected == N; ++i)
                    {
                        bool ok = true;
                        for (size_t j = 0; j < count && ok; ++j)
                            ok = isFreeInPattern(i + j);
                        if (ok)
                            expected = i;
                    }

                    buildState(order == 0);
                    const uint64_t got = allocateContiguous(count);
                    if (expected == N)
                        assert(got == 0);
                    else
                        assert(got == addr(expected));
                }

                // Releases below the hint: free each allocated frame and
                // require the next allocate() to return the lowest free one.
                for (size_t k = 0; k < m; ++k)
                {
                    if (pattern & (1u << k))
                        continue;                   // free already

                    buildState(order == 0);

                    // Advance the hint to the top before releasing.
                    uint64_t lowestFree = 0;
                    const uint64_t extra = allocate();   // may be 0 if full
                    (void)extra;

                    const bool extraTaken = (extra != 0);
                    const bool released = release(addr(managed[k]));
                    assert(released);

                    // Lowest free frame now: either the released one, or an
                    // earlier free frame from the pattern.
                    for (size_t i = 0; i < N && lowestFree == 0; ++i)
                    {
                        const bool freeNow =
                            (i == managed[k]) ||
                            (isFreeInPattern(i) &&
                             !(extraTaken && addr(i) == extra));
                        if (freeNow)
                            lowestFree = addr(i);
                    }

                    assert(allocate() == lowestFree);
                }

                // verify() is O(262144), so run it once per pattern/order
                // rather than after every step.
                buildState(order == 0);
                assert(verify());
            }
        }
    }

    // ---- 6. verify() catches corrupted bookkeeping --------------------
    void testVerifyDetectsCorruption()
    {
        Entry map[] = {{0x100000, 0x10000, 1, 1}};
        Header header = headerFor(1);

        auto fresh = [&]() { assert(init(header, map, nullptr, 0)); };

        fresh();
        assert(verify());

        // Allocated bit set but frame marked available.
        fresh();
        assert(allocate() == 0x100000);
        Stats beforeCheck;
        assert(getStats(beforeCheck));
        assert(verify());
        Stats afterCheck;
        assert(getStats(afterCheck));
        assert(afterCheck.freeFrames == beforeCheck.freeFrames);
        assert(afterCheck.framesAllocatedTotal ==
               beforeCheck.framesAllocatedTotal);
        assert(afterCheck.framesReleasedTotal ==
               beforeCheck.framesReleasedTotal);
        assert(afterCheck.failedAllocations == beforeCheck.failedAllocations);
        clear_bit(bitmap, 0x100);
        assert(!verify());

        // Free-frame counter drifts.
        fresh();
        ++free_frames;
        assert(!verify());

        fresh();
        --free_frames;
        assert(!verify());

        // Managed count no longer adds up.
        fresh();
        ++managed_frames;
        assert(!verify());

        // Frame 0 marked free.
        fresh();
        clear_bit(bitmap, 0);
        ++free_frames;
        ++managed_frames;
        assert(!verify());

        // A free frame below the search hint.
        fresh();
        search_hint = 0x108;
        assert(!verify());

        // Spurious allocated bit on a never-managed frame.
        fresh();
        set_bit(allocated, 5);
        assert(!verify());

        fresh();
        assert(verify());
    }

    // ---- 7. Model-based randomised test --------------------------------
    //
    // Reference model: one state per frame (reserved/free/allocated) in a
    // simple vector. Every operation's result is compared with what the
    // documented "lowest fit" semantics require; verify() is run
    // periodically.

    enum : char { RESERVED = 0, FREE = 1, ALLOCATED = 2 };

    void testRandomised(uint64_t seed)
    {
        constexpr size_t FIRST = 0x100;      // frame index of 0x100000
        constexpr size_t FRAMES = 512;       // 2 MiB window

        Entry map[] = {{FIRST * FRAME_SIZE, FRAMES * FRAME_SIZE, 1, 1}};
        Header header = headerFor(1);

        Rng rng{seed};

        // Random reserved holes.
        std::vector<ReservedRange> holes;
        for (int i = 0; i < 6; ++i)
        {
            const uint64_t at = FIRST + rng.below(FRAMES);
            const uint64_t len = 1 + rng.below(6);
            holes.push_back({at * FRAME_SIZE, len * FRAME_SIZE});
        }

        assert(init(header, map, holes.data(), holes.size()));

        std::vector<char> model(FRAMES, FREE);
        for (const auto& h : holes)
        {
            for (uint64_t a = h.base; a < h.base + h.length; a += FRAME_SIZE)
            {
                const size_t idx = static_cast<size_t>(a / FRAME_SIZE) - FIRST;
                if (idx < FRAMES)
                    model[idx] = RESERVED;
            }
        }

        auto modelFree = [&]() {
            size_t n = 0;
            for (char c : model) n += (c == FREE);
            return n;
        };

        // Lowest run of 'count' free frames, or FRAMES if none.
        auto modelFind = [&](size_t count) -> size_t {
            for (size_t i = 0; i + count <= FRAMES; ++i)
            {
                bool ok = true;
                for (size_t j = 0; j < count && ok; ++j)
                    ok = model[i + j] == FREE;
                if (ok) return i;
            }
            return FRAMES;
        };

        assert(freeFrameCount() == modelFree());

        for (int step = 0; step < 6000; ++step)
        {
            const size_t op = rng.below(10);

            if (op < 3)
            {
                // allocate()
                const size_t idx = modelFind(1);
                const uint64_t got = allocate();
                if (idx == FRAMES)
                {
                    assert(got == 0);
                }
                else
                {
                    assert(got == (FIRST + idx) * FRAME_SIZE);
                    model[idx] = ALLOCATED;
                }
            }
            else if (op < 5)
            {
                // allocateContiguous(1..8)
                const size_t count = 1 + rng.below(8);
                const size_t idx = modelFind(count);
                const uint64_t got = allocateContiguous(count);
                if (idx == FRAMES)
                {
                    assert(got == 0);
                }
                else
                {
                    assert(got == (FIRST + idx) * FRAME_SIZE);
                    for (size_t j = 0; j < count; ++j)
                        model[idx + j] = ALLOCATED;
                }
            }
            else if (op < 8)
            {
                // release() of a random frame: valid only if allocated.
                const size_t idx = rng.below(FRAMES);
                const bool expect = model[idx] == ALLOCATED;
                const bool got = release((FIRST + idx) * FRAME_SIZE);
                assert(got == expect);
                if (expect)
                    model[idx] = FREE;
            }
            else
            {
                // releaseContiguous() of a random range.
                const size_t idx = rng.below(FRAMES);
                const size_t count = 1 + rng.below(8);

                bool expect = idx + count <= FRAMES;
                for (size_t j = 0; expect && j < count; ++j)
                    expect = model[idx + j] == ALLOCATED;

                const bool got =
                    releaseContiguous((FIRST + idx) * FRAME_SIZE, count);
                assert(got == expect);
                if (expect)
                    for (size_t j = 0; j < count; ++j)
                        model[idx + j] = FREE;
            }

            assert(freeFrameCount() == modelFree());

            if (step % 100 == 0)
                assert(verify());
        }

        assert(verify());

        // Drain everything, release everything, and confirm a clean slate.
        while (allocate() != 0) {}
        assert(freeFrameCount() == 0);

        Stats s;
        assert(getStats(s));
        assert(s.freeFrames == 0);
        assert(s.allocatedFrames == s.managedFrames);

        for (size_t idx = 0; idx < FRAMES; ++idx)
        {
            if (isAllocated((FIRST + idx) * FRAME_SIZE))
                assert(release((FIRST + idx) * FRAME_SIZE));
        }

        assert(getStats(s));
        assert(s.allocatedFrames == 0);
        assert(s.freeFrames == s.managedFrames);
        assert(s.framesAllocatedTotal == s.framesReleasedTotal);
        assert(verify());
    }
}

int main()
{
    // Not initialised yet.
    assert(!verify());
    assert(!isAllocated(0x1000));
    assert(!isFree(0x1000));
    assert(allocateContiguous(1) == 0);
    assert(!releaseContiguous(0x1000, 1));
    Stats none;
    assert(!getStats(none));

    testFrameZeroReserved();
    testStats();
    testUntracked();
    testContiguous();
    testOrderAndHint();
    testHintExhaustive();
    testVerifyDetectsCorruption();

    for (uint64_t seed = 1; seed <= 8; ++seed)
        testRandomised(seed * 0x9E3779B97F4A7C15ULL);

    std::puts("Frame allocator hardening tests passed.");
    return 0;
}
