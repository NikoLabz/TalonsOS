#include "frame_allocator.h"

namespace
{
    uint8_t bitmap[talons_frames::BITMAP_SIZE];
    uint8_t allocated[talons_frames::BITMAP_SIZE];
    size_t free_frames = 0;
    bool initialised = false;

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

    initialised = true;
    return true;
}

uint64_t talons_frames::allocate()
{
    if (!initialised)
        return 0;

    for (size_t frame = 0; frame < MAX_FRAMES; ++frame)
    {
        if (!bit_is_set(bitmap, frame))
        {
            set_bit(bitmap, frame);
            set_bit(allocated, frame);
            --free_frames;
            return static_cast<uint64_t>(frame) * FRAME_SIZE;
        }
    }

    return 0;
}

bool talons_frames::release(uint64_t physicalAddress)
{
    if (!initialised ||
        physicalAddress == 0 ||
        physicalAddress % FRAME_SIZE != 0 ||
        physicalAddress >= MAX_PHYSICAL_ADDRESS)
        return false;

    const size_t frame =
        static_cast<size_t>(physicalAddress / FRAME_SIZE);

    if (!bit_is_set(allocated, frame))
        return false;

    clear_bit(allocated, frame);
    clear_bit(bitmap, frame);
    ++free_frames;
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
