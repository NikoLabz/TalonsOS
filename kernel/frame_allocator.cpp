#include "frame_allocator.h"

namespace
{
    uint8_t bitmap[talons_frames::BITMAP_SIZE];
    uint8_t allocated[talons_frames::BITMAP_SIZE];
    size_t free_frames = 0;
    bool initialised = false;

    // Phase 5 M2 state.
    size_t managed_frames = 0;
    // Invariant: no frame below search_hint is free.
    size_t search_hint = 0;
    uint64_t frames_allocated_total = 0;
    uint64_t frames_released_total = 0;
    uint64_t failed_allocations = 0;
    uint64_t rejected_releases = 0;
    uint64_t untracked_usable_bytes = 0;

    bool bit_is_set(const uint8_t* bits, size_t frame)
    {
        return (bits[frame / 8] &
                static_cast<uint8_t>(1u << (frame % 8))) != 0;
    }

    void set_bit(uint8_t* bits, size_t frame)
    {
        bits[frame / 8] |=
            static_cast<uint8_t>(1u << (frame % 8));
    }

    void clear_bit(uint8_t* bits, size_t frame)
    {
        bits[frame / 8] &=
            static_cast<uint8_t>(~(1u << (frame % 8)));
    }

    uint64_t saturating_add(uint64_t a, uint64_t b)
    {
        return a > UINT64_MAX - b ? UINT64_MAX : a + b;
    }

    // Lowest free frame at or after 'from', or MAX_FRAMES if none.
    // Skips fully-allocated bytes eight frames at a time.
    size_t find_free(size_t from)
    {
        size_t frame = from;

        while (frame < talons_frames::MAX_FRAMES)
        {
            if (frame % 8 == 0 && bitmap[frame / 8] == 0xFF)
            {
                frame += 8;
                continue;
            }

            if (!bit_is_set(bitmap, frame))
                return frame;

            ++frame;
        }

        return talons_frames::MAX_FRAMES;
    }

    void reserve_frame(size_t frame)
    {
        if (frame >= talons_frames::MAX_FRAMES)
            return;

        if (!bit_is_set(bitmap, frame))
        {
            set_bit(bitmap, frame);
            --free_frames;
        }
    }

    bool overlaps(
        uint64_t aBase, uint64_t aLength,
        uint64_t bBase, uint64_t bLength)
    {
        if (aLength == 0 || bLength == 0)
            return false;

        // Inputs have already been checked for overflow.
        return aBase < bBase + bLength &&
               bBase < aBase + aLength;
    }

    void reserve_range(uint64_t base, uint64_t length)
    {
        if (length == 0 || base >= talons_frames::MAX_PHYSICAL_ADDRESS)
            return;

        const uint64_t limit = talons_frames::MAX_PHYSICAL_ADDRESS;
        const uint64_t endAddress =
            length > limit - base ? limit : base + length;

        if (endAddress <= base)
            return;

        const uint64_t first = base / talons_frames::FRAME_SIZE;
        const uint64_t last =
            (endAddress - 1) / talons_frames::FRAME_SIZE;

        for (uint64_t frame = first; frame <= last; ++frame)
            reserve_frame(static_cast<size_t>(frame));
    }
}

bool talons_frames::init(
    const talons_e820::Header& header,
    const talons_e820::Entry* entries,
    const ReservedRange* reserved,
    size_t reservedCount)
{
    initialised = false;
    free_frames = 0;
    managed_frames = 0;
    search_hint = 0;
    frames_allocated_total = 0;
    frames_released_total = 0;
    failed_allocations = 0;
    rejected_releases = 0;
    untracked_usable_bytes = 0;

    for (size_t i = 0; i < BITMAP_SIZE; ++i)
    {
        bitmap[i] = 0xFF;
        allocated[i] = 0;
    }

    if (!talons_e820::validMap(header, entries))
        return false;

    if (reservedCount != 0 && reserved == nullptr)
        return false;

    for (size_t i = 0; i < reservedCount; ++i)
    {
        if (reserved[i].length == 0 ||
            reserved[i].base > UINT64_MAX - reserved[i].length)
            return false;
    }

    // Every frame starts unavailable. Only complete, unambiguous
    // frames inside enabled usable E820 entries can become available.
    for (uint32_t i = 0; i < header.count; ++i)
    {
        const auto& entry = entries[i];

        if (!talons_e820::isUsable(entry))
            continue;

        const uint64_t entryEnd = entry.base + entry.length;

        if (entryEnd > MAX_PHYSICAL_ADDRESS)
        {
            const uint64_t from = entry.base > MAX_PHYSICAL_ADDRESS
                ? entry.base
                : MAX_PHYSICAL_ADDRESS;

            untracked_usable_bytes = saturating_add(
                untracked_usable_bytes, entryEnd - from);
        }

        const uint64_t first =
            entry.base / FRAME_SIZE +
            (entry.base % FRAME_SIZE != 0 ? 1 : 0);
        const uint64_t last = entryEnd / FRAME_SIZE;

        for (uint64_t frame = first; frame < last && frame < MAX_FRAMES;
             ++frame)
        {
            const uint64_t frameBase = frame * FRAME_SIZE;
            bool usable = true;

            for (uint32_t j = 0; j < header.count; ++j)
            {
                const auto& other = entries[j];

                if (other.length == 0)
                    continue;

                if (other.base > UINT64_MAX - other.length)
                    return false;

                // Any overlap with another entry makes the frame
                // ambiguous, regardless of the overlapping type.
                if (j != i &&
                    overlaps(frameBase, FRAME_SIZE,
                             other.base, other.length))
                {
                    usable = false;
                    break;
                }
            }

            if (usable)
            {
                const size_t index = static_cast<size_t>(frame);
                if (bit_is_set(bitmap, index))
                {
                    clear_bit(bitmap, index);
                    ++free_frames;
                }
            }
        }
    }

    // Explicit OS-reserved ranges always override usable memory.
    for (size_t i = 0; i < reservedCount; ++i)
        reserve_range(reserved[i].base, reserved[i].length);

    // Address 0 is allocate()'s failure value; it must never be handed out.
    reserve_frame(0);

    managed_frames = free_frames;
    search_hint = 0;

    initialised = true;
    return true;
}

uint64_t talons_frames::allocate()
{
    if (!initialised)
        return 0;

    const size_t frame = find_free(search_hint);

    if (frame >= MAX_FRAMES)
    {
        ++failed_allocations;
        return 0;
    }

    set_bit(bitmap, frame);
    set_bit(allocated, frame);
    --free_frames;
    ++frames_allocated_total;
    search_hint = frame + 1;
    return static_cast<uint64_t>(frame) * FRAME_SIZE;
}

bool talons_frames::release(uint64_t physicalAddress)
{
    if (!initialised)
        return false;

    if (physicalAddress == 0 ||
        physicalAddress % FRAME_SIZE != 0 ||
        physicalAddress >= MAX_PHYSICAL_ADDRESS)
    {
        ++rejected_releases;
        return false;
    }

    const size_t frame =
        static_cast<size_t>(physicalAddress / FRAME_SIZE);

    if (!bit_is_set(allocated, frame))
    {
        ++rejected_releases;
        return false;
    }

    clear_bit(allocated, frame);
    clear_bit(bitmap, frame);
    ++free_frames;
    ++frames_released_total;

    if (frame < search_hint)
        search_hint = frame;

    return true;
}

size_t talons_frames::freeFrameCount()
{
    return initialised ? free_frames : 0;
}

bool talons_frames::isInitialised()
{
    return initialised;
}

// ---------------------------------------------------------------------
// Phase 5 M2 additions
// ---------------------------------------------------------------------

bool talons_frames::getStats(Stats& out)
{
    out = Stats{};

    if (!initialised)
        return false;

    out.managedFrames = managed_frames;
    out.freeFrames = free_frames;
    out.allocatedFrames = managed_frames - free_frames;
    out.framesAllocatedTotal = frames_allocated_total;
    out.framesReleasedTotal = frames_released_total;
    out.failedAllocations = failed_allocations;
    out.rejectedReleases = rejected_releases;
    out.untrackedUsableBytes = untracked_usable_bytes;

    uint64_t run = 0;
    uint64_t best = 0;

    for (size_t frame = 0; frame < MAX_FRAMES; ++frame)
    {
        if (!bit_is_set(bitmap, frame))
        {
            ++run;
            if (run > best)
                best = run;
        }
        else
        {
            run = 0;
        }
    }

    out.largestFreeRun = best;
    return true;
}

uint64_t talons_frames::allocateContiguous(size_t count)
{
    if (!initialised || count == 0)
        return 0;

    if (count > MAX_FRAMES)
    {
        ++failed_allocations;
        return 0;
    }

    size_t start = find_free(search_hint);
    const size_t firstFree = start;

    while (start < MAX_FRAMES)
    {
        size_t run = 1;

        while (run < count &&
               start + run < MAX_FRAMES &&
               !bit_is_set(bitmap, start + run))
            ++run;

        if (run == count)
        {
            for (size_t i = 0; i < count; ++i)
            {
                set_bit(bitmap, start + i);
                set_bit(allocated, start + i);
            }

            free_frames -= count;
            frames_allocated_total += count;

            // Frames below 'start' that were free before still are, so
            // the hint may only advance past a run that began at the
            // lowest free frame.
            search_hint = (start == firstFree) ? start + count : firstFree;

            return static_cast<uint64_t>(start) * FRAME_SIZE;
        }

        // Frame start+run is allocated/reserved, or the limit was hit.
        start = find_free(start + run);
    }

    ++failed_allocations;
    return 0;
}

bool talons_frames::releaseContiguous(
    uint64_t physicalAddress, size_t count)
{
    if (!initialised)
        return false;

    if (count == 0 ||
        physicalAddress == 0 ||
        physicalAddress % FRAME_SIZE != 0 ||
        physicalAddress >= MAX_PHYSICAL_ADDRESS ||
        count > MAX_FRAMES ||
        count > (MAX_PHYSICAL_ADDRESS - physicalAddress) / FRAME_SIZE)
    {
        ++rejected_releases;
        return false;
    }

    const size_t first = static_cast<size_t>(physicalAddress / FRAME_SIZE);

    // Validate the whole range before changing anything.
    for (size_t i = 0; i < count; ++i)
    {
        if (!bit_is_set(allocated, first + i))
        {
            ++rejected_releases;
            return false;
        }
    }

    for (size_t i = 0; i < count; ++i)
    {
        clear_bit(allocated, first + i);
        clear_bit(bitmap, first + i);
    }

    free_frames += count;
    frames_released_total += count;

    if (first < search_hint)
        search_hint = first;

    return true;
}

bool talons_frames::isAllocated(uint64_t physicalAddress)
{
    if (!initialised ||
        physicalAddress % FRAME_SIZE != 0 ||
        physicalAddress >= MAX_PHYSICAL_ADDRESS)
        return false;

    return bit_is_set(
        allocated, static_cast<size_t>(physicalAddress / FRAME_SIZE));
}

bool talons_frames::isFree(uint64_t physicalAddress)
{
    if (!initialised ||
        physicalAddress % FRAME_SIZE != 0 ||
        physicalAddress >= MAX_PHYSICAL_ADDRESS)
        return false;

    return !bit_is_set(
        bitmap, static_cast<size_t>(physicalAddress / FRAME_SIZE));
}

bool talons_frames::verify()
{
    if (!initialised)
        return false;

    size_t freeCount = 0;
    size_t allocatedCount = 0;

    for (size_t frame = 0; frame < MAX_FRAMES; ++frame)
    {
        const bool unavailable = bit_is_set(bitmap, frame);
        const bool isAlloc = bit_is_set(allocated, frame);

        if (isAlloc)
        {
            ++allocatedCount;

            if (!unavailable)
                return false;
        }

        if (!unavailable)
        {
            ++freeCount;

            if (frame == 0 || frame < search_hint)
                return false;
        }
    }

    return freeCount == free_frames &&
           freeCount + allocatedCount == managed_frames;
}
