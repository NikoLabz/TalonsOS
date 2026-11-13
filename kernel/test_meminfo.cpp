// Integration test: the real kernel/meminfo.cpp and the real
// kernel/frame_allocator.cpp, with the E820 accessors, the stream layer
// and _kernel_end replaced by host stubs that capture output.

#include "e820.h"
#include "frame_allocator.h"
#include "memory.h"
#include "meminfo.h"
#include "standard_streams.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
    constexpr int OUT = 1;
    constexpr int ERR = 2;

    std::string capturedOut;
    std::string capturedErr;

    bool haveMap = false;
    char diagnosticScreen[2000] = {};
    unsigned short diagnosticPosition = 0;

    talons_e820::Entry testEntries[] = {
        {0x0,         0x9FC00,      1, 1},
        {0x9FC00,     0x400,        2, 1},
        {0xF0000,     0x10000,      2, 1},
        {0x100000,    0x7EE0000,    1, 1},
        {0x7FE0000,   0x20000,      2, 1},
        {0xFFFC0000,  0x40000,      2, 1},
    };

    talons_e820::Header testHeader{
        talons_e820::MAGIC, 6, talons_e820::ENTRY_SIZE, 0};

    void reset()
    {
        capturedOut.clear();
        capturedErr.clear();
        std::memset(diagnosticScreen, 0, sizeof(diagnosticScreen));
        diagnosticPosition = 0;
    }

    bool contains(const std::string& text, const char* needle)
    {
        return text.find(needle) != std::string::npos;
    }

    bool screenContains(const char* needle)
    {
        for (int row = 0; row < 25; ++row)
            if (std::strstr(&diagnosticScreen[row * 80], needle) != nullptr)
                return true;
        return false;
    }
}

unsigned short terminal_get_output_position()
{
    return diagnosticPosition;
}

void terminal_set_output_position(unsigned short position)
{
    diagnosticPosition = position;
}

void vga_putchar_at(int position, char c, unsigned char)
{
    if (position >= 0 && position < 2000)
        diagnosticScreen[position] = c;
}

void vga_scroll(unsigned char)
{
    for (int i = 80; i < 2000; ++i)
        diagnosticScreen[i - 80] = diagnosticScreen[i];
    for (int i = 1920; i < 2000; ++i)
        diagnosticScreen[i] = ' ';
}

// Stubs for symbols meminfo.cpp references.
extern "C" char _kernel_end;
char _kernel_end;

const talons_e820::Header* e820_header()
{
    return haveMap ? &testHeader : nullptr;
}

const talons_e820::Entry* e820_entries()
{
    return haveMap ? testEntries : nullptr;
}

bool stream_write_string(int fd, const char* string)
{
    if (fd == OUT)
        capturedOut += string;
    else if (fd == ERR)
        capturedErr += string;
    else
        return false;

    return true;
}

int main()
{
    // 1. No valid map: error on stderr, nothing on stdout.
    haveMap = false;
    reset();
    meminfo_run(OUT, ERR);
    assert(capturedOut.empty());
    assert(capturedErr == "meminfo: no valid E820 memory map\n");

    reset();
    memmap_run(OUT, ERR);
    assert(capturedOut.empty());
    assert(capturedErr == "memmap: no valid E820 memory map\n");

    // 2. Valid map, frame allocator NOT initialised yet.
    haveMap = true;
    assert(!talons_frames::isInitialised());
    reset();
    meminfo_run(OUT, ERR);
    assert(capturedErr.empty());
    assert(contains(capturedOut, "  Entries:         6\n"));
    assert(contains(capturedOut,
        "  Usable:          133692416 bytes (127 MiB)\n"));
    assert(contains(capturedOut,
        "  Reserved:        459776 bytes (449 KiB)\n"));
    assert(contains(capturedOut,
        "  Highest usable:  0x07FE0000 (end, exclusive)\n"));
    assert(contains(capturedOut, "  Overlaps:        no\n"));
    assert(contains(capturedOut, "  State:           not initialised\n"));
    talons_heap::Stats uninitialisedHeap{};
    assert(!memory_get_stats(uninitialisedHeap));
    assert(!contains(capturedOut, "Disabled"));

    // 3. Initialise the real frame allocator from the same map.
    const talons_frames::ReservedRange reserved[] = {
        {0x0,      0x10000},
        {0x10000,  0x70000},
        {0x80000,  0x13000},
        {0x100000, 0x100000},
    };

    assert(talons_frames::init(testHeader, testEntries, reserved, 4));
    const size_t freeFrames = talons_frames::freeFrameCount();
    assert(freeFrames > 0);

    reset();
    meminfo_run(OUT, ERR);
    assert(capturedErr.empty());

    char expected[128];
    std::snprintf(expected, sizeof(expected), "  Free frames:     %zu (",
                  freeFrames);
    assert(contains(capturedOut, expected));
    assert(contains(capturedOut, "  Tracked up to:   0x40000000\n"));
    assert(contains(capturedOut, "Kernel\n  Image end:       0x"));
    assert(contains(capturedOut,
        "Kernel heap\n  State:           not initialised\n"));

    // 3b. M2 statistics lines.
    {
        talons_frames::Stats stats;
        assert(talons_frames::getStats(stats));

        char line[160];
        std::snprintf(line, sizeof(line), "  Managed frames:  %llu\n",
                      static_cast<unsigned long long>(stats.managedFrames));
        assert(contains(capturedOut, line));
        assert(contains(capturedOut, "  Allocated:       0 frames\n"));
        assert(contains(capturedOut,
            "  Lifetime:        0 allocated, 0 released\n"));
        assert(contains(capturedOut,
            "  Failures:        0 allocations failed, 0 releases rejected\n"));
        assert(contains(capturedOut, "  Self-check:      OK\n"));
        assert(!contains(capturedOut, "Untracked"));

    memory_init();

    // memcheck writes directly to VGA-backed output and leaves allocator
    // counters unchanged while reporting both integrity results.
    talons_frames::Stats pmmBeforeCheck{};
    assert(talons_frames::getStats(pmmBeforeCheck));
    talons_heap::Stats heapBeforeCheck{};
    assert(memory_get_stats(heapBeforeCheck));
    memcheck_run();
    assert(screenContains("Memory integrity"));
    assert(screenContains("PMM: OK"));
    assert(screenContains("Heap: OK"));
    talons_frames::Stats pmmAfterCheck{};
    talons_heap::Stats heapAfterCheck{};
    assert(talons_frames::getStats(pmmAfterCheck));
    assert(memory_get_stats(heapAfterCheck));
    assert(pmmAfterCheck.freeFrames == pmmBeforeCheck.freeFrames);
    assert(pmmAfterCheck.framesAllocatedTotal ==
           pmmBeforeCheck.framesAllocatedTotal);
    assert(pmmAfterCheck.framesReleasedTotal ==
           pmmBeforeCheck.framesReleasedTotal);
    assert(heapAfterCheck.totalAllocations == heapBeforeCheck.totalAllocations);
    assert(heapAfterCheck.totalFrees == heapBeforeCheck.totalFrees);

    void* damaged = kmalloc(8);
    assert(damaged != nullptr);
    static_cast<unsigned char*>(damaged)[8] = 0;
    reset();
    memcheck_run();
    assert(screenContains("Heap: FAILED"));
    assert(screenContains("canaries: 1"));
    memory_init();
    }

    // Counters follow real allocator activity.
    {
        const uint64_t a = talons_frames::allocate();
        const uint64_t b = talons_frames::allocate();
        assert(a != 0 && b != 0);
        assert(talons_frames::release(a));
        assert(!talons_frames::release(a));          // rejected double free
        assert(talons_frames::allocateContiguous(1u << 30) == 0); // fails

        reset();
        meminfo_run(OUT, ERR);
        assert(contains(capturedOut, "  Allocated:       1 frames\n"));
        assert(contains(capturedOut,
            "  Lifetime:        2 allocated, 1 released\n"));
        assert(contains(capturedOut,
            "  Failures:        1 allocations failed, 1 releases rejected\n"));
        assert(contains(capturedOut, "  Self-check:      OK\n"));

        assert(talons_frames::release(b));
    }

    // Usable memory above the managed limit is reported.
    {
        talons_e820::Entry highMap[] = {
            {0x100000, 0x4000, 1, 1},
            {talons_frames::MAX_PHYSICAL_ADDRESS + 0x100000, 0x200000, 1, 1},
        };
        talons_e820::Header highHeader{
            talons_e820::MAGIC, 2, talons_e820::ENTRY_SIZE, 0};

        assert(talons_frames::init(highHeader, highMap, nullptr, 0));

        reset();
        meminfo_run(OUT, ERR);
        assert(contains(capturedOut,
            "  Untracked:       2097152 bytes (2 MiB) usable above the limit\n"));

        // Restore the standard state for the remaining checks.
        assert(talons_frames::init(testHeader, testEntries, reserved, 4));
    }

    // 3c. M3: kernel heap section (real memory.cpp + heap.cpp, host heap).
    {
        memory_init();

        reset();
        meminfo_run(OUT, ERR);
        assert(capturedErr.empty());
        assert(contains(capturedOut, "Kernel heap\n"));
        {
            const std::string healthy =
                capturedOut.substr(capturedOut.find("Kernel heap\n"));
            assert(healthy.find("  Self-check:      OK\n") !=
                   std::string::npos);
        }
        assert(contains(capturedOut, "  Size:            1048576 bytes (1 MiB)\n"));
        assert(contains(capturedOut,
            "  In use:          0 bytes in 0 blocks (peak 0 bytes)\n"));
        assert(contains(capturedOut, "in 1 blocks, largest 1048544 bytes\n"));
        assert(contains(capturedOut, "  Fragmentation:   0%\n"));
        assert(contains(capturedOut,
            "  Lifetime:        0 allocations, 0 frees\n"));
        assert(contains(capturedOut,
            "  Failures:        0 out of memory, 0 double frees, "
            "0 invalid frees\n"));
        assert(contains(capturedOut,
            "  Corruption:      0 headers, 0 overflows, 0 use-after-free\n"));

        void* a = kmalloc(1000);
        void* b = kmalloc(500);
        assert(a && b);
        kfree(a);
        kfree(a);                                    // double free
        assert(kmalloc(memory_max_allocation() + 1) == nullptr);   // OOM

        reset();
        meminfo_run(OUT, ERR);
        assert(contains(capturedOut,
            "  In use:          500 bytes in 1 blocks (peak 1500 bytes)\n"));
        assert(contains(capturedOut,
            "  Lifetime:        2 allocations, 1 frees\n"));
        assert(contains(capturedOut,
            "  Failures:        1 out of memory, 1 double frees, "
            "0 invalid frees\n"));
        assert(contains(capturedOut, "  Fragmentation:   "));

        // Memory damage turns the heap self-check red.
        static_cast<unsigned char*>(b)[500] = 0;     // clobber slack
        reset();
        meminfo_run(OUT, ERR);
        assert(contains(capturedOut, "Kernel heap\n"));
        const std::string heapPart =
            capturedOut.substr(capturedOut.find("Kernel heap\n"));
        assert(heapPart.find("  Self-check:      FAILED\n") !=
               std::string::npos);

        memory_init();
    }

    // 4. memmap: header plus one line per entry, exact text.
    reset();
    memmap_run(OUT, ERR);
    assert(capturedErr.empty());

    const std::string expectedMap =
        "Idx  Start               End (inclusive)     Type\n"
        "00   0x0000000000000000  0x000000000009FBFF  Usable\n"
        "01   0x000000000009FC00  0x000000000009FFFF  Reserved\n"
        "02   0x00000000000F0000  0x00000000000FFFFF  Reserved\n"
        "03   0x0000000000100000  0x0000000007FDFFFF  Usable\n"
        "04   0x0000000007FE0000  0x0000000007FFFFFF  Reserved\n"
        "05   0x00000000FFFC0000  0x00000000FFFFFFFF  Reserved\n";
    assert(capturedOut == expectedMap);

    // 5. Empty, disabled and other-type entries are labelled; overlap is
    //    reported by meminfo.
    testEntries[1] = {0x9FC00, 0, 2, 1};            // empty
    testEntries[2] = {0xF0000, 0x10000, 3, 1};      // ACPI reclaimable
    testEntries[4] = {0x7FE0000, 0x20000, 4, 0};    // ACPI NVS, disabled
    testEntries[5] = {0x80000, 0x2000, 5, 1};       // bad memory, overlaps #0

    reset();
    memmap_run(OUT, ERR);
    assert(contains(capturedOut, "01   (empty entry)\n"));
    assert(contains(capturedOut,
        "02   0x00000000000F0000  0x00000000000FFFFF  ACPI reclaimable\n"));
    assert(contains(capturedOut,
        "04   0x0000000007FE0000  0x0000000007FFFFFF  ACPI NVS (disabled)\n"));
    assert(contains(capturedOut,
        "05   0x0000000000080000  0x0000000000081FFF  Bad memory\n"));

    reset();
    meminfo_run(OUT, ERR);
    assert(contains(capturedOut, "  Disabled:        131072 bytes (128 KiB)\n"));
    assert(contains(capturedOut,
        "  Overlaps:        yes (totals may double count)\n"));

    // 6. Writes go to the file descriptors the caller supplied.
    reset();
    meminfo_run(7, 8);   // unknown fds: stub rejects, nothing captured
    assert(capturedOut.empty() && capturedErr.empty());

    std::puts("meminfo/memmap integration tests passed.");
    return 0;
}
