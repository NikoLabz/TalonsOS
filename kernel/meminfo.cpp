#include "meminfo.h"

#include "e820.h"
#include "frame_allocator.h"
#include "memory.h"
#include "memory_format.h"
#include "memory_map.h"
#include "standard_streams.h"
#include "terminal.h"
#include "vga.h"

#include <stddef.h>
#include <stdint.h>

extern "C" char _kernel_end;

namespace
{
    void put(int fd, const char* text)
    {
        stream_write_string(fd, text);
    }

    void putDecimal(int fd, uint64_t value)
    {
        char buffer[24];
        if (talons_memfmt::formatDecimal(value, buffer, sizeof(buffer)) != 0)
            put(fd, buffer);
    }

    void putHex(int fd, uint64_t value, unsigned digits)
    {
        char buffer[24];
        if (talons_memfmt::formatHex(value, digits, buffer,
                                     sizeof(buffer)) != 0)
            put(fd, buffer);
    }

    // "<bytes> bytes (<size>)" on the current line, no newline.
    void putBytesAndSize(int fd, uint64_t bytes)
    {
        char size[32];

        putDecimal(fd, bytes);
        put(fd, " bytes");

        if (bytes >= 1024 &&
            talons_memfmt::formatSize(bytes, size, sizeof(size)) != 0)
        {
            put(fd, " (");
            put(fd, size);
            put(fd, ")");
        }
    }

    bool loadMap(
        int stderrFd,
        const char* command,
        const talons_e820::Header*& header,
        const talons_e820::Entry*& entries)
    {
        header = e820_header();
        entries = e820_entries();

        if (header == nullptr || entries == nullptr)
        {
            put(stderrFd, command);
            put(stderrFd, ": no valid E820 memory map\n");
            return false;
        }

        return true;
    }

    void screenString(unsigned short& position, const char* text,
                      unsigned char colour)
    {
        for (size_t i = 0; text[i] != '\0'; ++i)
        {
            if (text[i] == '\n')
            {
                position = static_cast<unsigned short>(
                    ((position / 80) + 1) * 80);
                continue;
            }
            if (position >= 2000)
            {
                vga_scroll(colour);
                position = static_cast<unsigned short>(position - 80);
            }
            vga_putchar_at(position++, text[i], colour);
        }
    }

    void screenNumber(unsigned short& position, uint64_t value,
                      unsigned char colour)
    {
        char buffer[24];
        if (talons_memfmt::formatDecimal(value, buffer, sizeof(buffer)) == 0)
            screenString(position, "?", colour);
        else
            screenString(position, buffer, colour);
    }

    const char* heapErrorName(talons_heap::Error error)
    {
        switch (error)
        {
        case talons_heap::Error::OutOfMemory: return "out of memory";
        case talons_heap::Error::DoubleFree: return "double free";
        case talons_heap::Error::InvalidFree: return "invalid free";
        case talons_heap::Error::HeaderCorruption: return "header corruption";
        case talons_heap::Error::Overflow: return "overflow";
        case talons_heap::Error::UseAfterFree: return "use-after-free";
        case talons_heap::Error::None: return "none";
        }
        return "unknown";
    }
}

void meminfo_run(int stdoutFd, int stderrFd)
{
    const talons_e820::Header* header = nullptr;
    const talons_e820::Entry* entries = nullptr;

    if (!loadMap(stderrFd, "meminfo", header, entries))
        return;

    talons_memmap::Summary summary;
    if (!talons_memmap::summarise(*header, entries, summary))
    {
        put(stderrFd, "meminfo: E820 memory map failed validation\n");
        return;
    }

    put(stdoutFd, "Physical memory (BIOS E820)\n");

    put(stdoutFd, "  Entries:         ");
    putDecimal(stdoutFd, summary.entryCount);
    put(stdoutFd, "\n");

    put(stdoutFd, "  Usable:          ");
    putBytesAndSize(stdoutFd, summary.usableBytes);
    put(stdoutFd, "\n");

    put(stdoutFd, "  Reserved:        ");
    putBytesAndSize(stdoutFd, summary.reservedBytes);
    put(stdoutFd, "\n");

    if (summary.disabledEntries != 0)
    {
        put(stdoutFd, "  Disabled:        ");
        putBytesAndSize(stdoutFd, summary.disabledBytes);
        put(stdoutFd, "\n");
    }

    put(stdoutFd, "  Highest usable:  ");
    putHex(stdoutFd, summary.highestUsableEnd, 8);
    put(stdoutFd, " (end, exclusive)\n");

    put(stdoutFd, "  Overlaps:        ");
    put(stdoutFd, summary.hasOverlap ? "yes (totals may double count)\n"
                                     : "no\n");

    put(stdoutFd, "Frame allocator (");
    putDecimal(stdoutFd, talons_frames::FRAME_SIZE);
    put(stdoutFd, "-byte frames)\n");

    if (!talons_frames::isInitialised())
    {
        put(stdoutFd, "  State:           not initialised\n");
    }
    else
    {
        const uint64_t freeFrames = talons_frames::freeFrameCount();

        put(stdoutFd, "  Free frames:     ");
        putDecimal(stdoutFd, freeFrames);
        put(stdoutFd, " (");
        putBytesAndSize(stdoutFd, freeFrames * talons_frames::FRAME_SIZE);
        put(stdoutFd, ")\n");

        talons_frames::Stats stats;
        if (talons_frames::getStats(stats))
        {
            put(stdoutFd, "  Managed frames:  ");
            putDecimal(stdoutFd, stats.managedFrames);
            put(stdoutFd, "\n");

            put(stdoutFd, "  Allocated:       ");
            putDecimal(stdoutFd, stats.allocatedFrames);
            put(stdoutFd, " frames\n");

            put(stdoutFd, "  Largest free run: ");
            putDecimal(stdoutFd, stats.largestFreeRun);
            put(stdoutFd, " frames\n");

            put(stdoutFd, "  Lifetime:        ");
            putDecimal(stdoutFd, stats.framesAllocatedTotal);
            put(stdoutFd, " allocated, ");
            putDecimal(stdoutFd, stats.framesReleasedTotal);
            put(stdoutFd, " released\n");

            put(stdoutFd, "  Failures:        ");
            putDecimal(stdoutFd, stats.failedAllocations);
            put(stdoutFd, " allocations failed, ");
            putDecimal(stdoutFd, stats.rejectedReleases);
            put(stdoutFd, " releases rejected\n");

            if (stats.untrackedUsableBytes != 0)
            {
                put(stdoutFd, "  Untracked:       ");
                putBytesAndSize(stdoutFd, stats.untrackedUsableBytes);
                put(stdoutFd, " usable above the limit\n");
            }
        }

        put(stdoutFd, "  Self-check:      ");
        put(stdoutFd, talons_frames::verify() ? "OK\n" : "FAILED\n");
    }

    put(stdoutFd, "  Tracked up to:   ");
    putHex(stdoutFd, talons_frames::MAX_PHYSICAL_ADDRESS, 8);
    put(stdoutFd, "\n");

    {
        talons_heap::Stats heap;
        const bool walked = memory_get_stats(heap);

        put(stdoutFd, "Kernel heap\n");

        if (!walked && heap.heapSize == 0)
        {
            put(stdoutFd, "  State:           not initialised\n");
        }
        else
        {
        put(stdoutFd, "  Size:            ");
        putBytesAndSize(stdoutFd, heap.heapSize);
        put(stdoutFd, "\n");

        put(stdoutFd, "  In use:          ");
        putBytesAndSize(stdoutFd, heap.bytesInUse);
        put(stdoutFd, " in ");
        putDecimal(stdoutFd, heap.allocatedBlocks);
        put(stdoutFd, " blocks (peak ");
        putDecimal(stdoutFd, heap.peakBytesInUse);
        put(stdoutFd, " bytes)\n");

        put(stdoutFd, "  Free:            ");
        putBytesAndSize(stdoutFd, heap.freeBytes);
        put(stdoutFd, " in ");
        putDecimal(stdoutFd, heap.freeBlocks);
        put(stdoutFd, " blocks, largest ");
        putDecimal(stdoutFd, heap.largestFreeBlock);
        put(stdoutFd, " bytes\n");

        put(stdoutFd, "  Fragmentation:   ");
        putDecimal(stdoutFd, heap.fragmentationPercent);
        put(stdoutFd, "%\n");

        put(stdoutFd, "  Lifetime:        ");
        putDecimal(stdoutFd, heap.totalAllocations);
        put(stdoutFd, " allocations, ");
        putDecimal(stdoutFd, heap.totalFrees);
        put(stdoutFd, " frees\n");

        put(stdoutFd, "  Failures:        ");
        putDecimal(stdoutFd, heap.outOfMemoryEvents);
        put(stdoutFd, " out of memory, ");
        putDecimal(stdoutFd, heap.doubleFrees);
        put(stdoutFd, " double frees, ");
        putDecimal(stdoutFd, heap.invalidFrees);
        put(stdoutFd, " invalid frees\n");

        put(stdoutFd, "  Corruption:      ");
        putDecimal(stdoutFd, heap.headerCorruptions);
        put(stdoutFd, " headers, ");
        putDecimal(stdoutFd, heap.overflowsDetected);
        put(stdoutFd, " overflows, ");
        putDecimal(stdoutFd, heap.useAfterFreeDetected);
        put(stdoutFd, " use-after-free\n");

        talons_heap::CheckReport report;
        const bool healthy = memory_check(report) && walked;

        put(stdoutFd, "  Self-check:      ");
        put(stdoutFd, healthy ? "OK\n" : "FAILED\n");
        }
    }

    put(stdoutFd, "Kernel\n");
    put(stdoutFd, "  Image end:       ");
    putHex(stdoutFd, reinterpret_cast<uintptr_t>(&_kernel_end), 8);
    put(stdoutFd, "\n");
}

void memmap_run(int stdoutFd, int stderrFd)
{
    const talons_e820::Header* header = nullptr;
    const talons_e820::Entry* entries = nullptr;

    if (!loadMap(stderrFd, "memmap", header, entries))
        return;

    put(stdoutFd, "Idx  Start               End (inclusive)     Type\n");

    for (uint32_t i = 0; i < header->count; ++i)
    {
        const talons_e820::Entry& entry = entries[i];

        if (i < 10)
            put(stdoutFd, "0");
        putDecimal(stdoutFd, i);
        put(stdoutFd, "   ");

        if (entry.length == 0)
        {
            put(stdoutFd, "(empty entry)\n");
            continue;
        }

        putHex(stdoutFd, entry.base, 16);
        put(stdoutFd, "  ");

        // length != 0 and base + length does not overflow (validMap).
        putHex(stdoutFd, entry.base + entry.length - 1, 16);
        put(stdoutFd, "  ");

        put(stdoutFd, talons_memmap::typeName(entry.type));

        if ((entry.attributes & 1u) == 0)
            put(stdoutFd, " (disabled)");

        put(stdoutFd, "\n");
    }
}

void memcheck_run()
{
    // This diagnostic intentionally bypasses file streams. It must remain
    // safe when a damaged heap is the subject of the report.
    const bool pmmReady = talons_frames::isInitialised();
    const bool pmmOk = pmmReady && talons_frames::verify();
    talons_heap::CheckReport heapReport{};
    const bool heapOk = memory_check(heapReport);
    talons_heap::Stats heapStats{};
    const bool heapStatsOk = memory_get_stats(heapStats);
    talons_frames::Stats frameStats{};
    const bool frameStatsOk = talons_frames::getStats(frameStats);
    const unsigned char colour = pmmOk && heapOk ? 0x0A : 0x0C;
    unsigned short position = terminal_get_output_position();

    screenString(position, "Memory integrity\nPMM: ", colour);
    screenString(position, !pmmReady ? "NOT INITIALISED\n" :
        (pmmOk ? "OK\n" : "FAILED\n"), colour);
    if (frameStatsOk)
    {
        screenString(position, "  Free: ", colour);
        screenNumber(position, frameStats.freeFrames, colour);
        screenString(position, ", failed allocations: ", colour);
        screenNumber(position, frameStats.failedAllocations, colour);
        screenString(position, "\n", colour);
    }

    screenString(position, "Heap: ", colour);
    screenString(position, !heapStatsOk && heapStats.heapSize == 0
        ? "NOT INITIALISED\n"
        : (heapStatsOk && heapOk ? "OK\n" : "FAILED\n"), colour);
    if (heapStatsOk)
    {
        screenString(position, "  OOM: ", colour);
        screenNumber(position, heapStats.outOfMemoryEvents, colour);
        screenString(position, ", double frees: ", colour);
        screenNumber(position, heapStats.doubleFrees, colour);
        screenString(position, ", invalid frees: ", colour);
        screenNumber(position, heapStats.invalidFrees, colour);
        screenString(position, "\nHeap errors: headers ", colour);
        screenNumber(position, heapStats.headerCorruptions, colour);
        screenString(position, ", overflows ", colour);
        screenNumber(position, heapStats.overflowsDetected, colour);
        screenString(position, ", use-after-free ", colour);
        screenNumber(position, heapStats.useAfterFreeDetected, colour);
        screenString(position, "\n", colour);
    }
    if (!heapOk)
    {
        screenString(position, "  Bad headers: ", colour);
        screenNumber(position, heapReport.badHeaders, colour);
        screenString(position, ", canaries: ", colour);
        screenNumber(position, heapReport.badCanaries, colour);
        screenString(position, ", poison: ", colour);
        screenNumber(position, heapReport.badPoison, colour);
        screenString(position, ", adjacent free: ", colour);
        screenNumber(position, heapReport.adjacentFree, colour);
        screenString(position, "\n", colour);
        screenString(position, "  First issue: ", colour);
        screenString(position, heapErrorName(heapReport.firstProblemKind), colour);
        screenString(position, " at ", colour);
        char address[24];
        if (talons_memfmt::formatHex(
                reinterpret_cast<uintptr_t>(heapReport.firstProblem), 16,
                address, sizeof(address)) != 0)
            screenString(position, address, colour);
        screenString(position, "\n", colour);
    }
    terminal_set_output_position(position);
}
