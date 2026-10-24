#include "memory.h"

namespace
{
    constexpr std::size_t HEAP_START = 0x100000;
    constexpr std::size_t HEAP_SIZE  = 0x100000;
    constexpr std::size_t ALIGNMENT  = 16;

    struct Allocation
    {
        void* address;
        std::size_t size;
        bool free;
    };

    constexpr std::size_t MAX_ALLOCATIONS = 256;

    Allocation allocations[MAX_ALLOCATIONS];

    std::size_t heap_position = 0;

    bool align_up(std::size_t value, std::size_t& result)
    {
        constexpr std::size_t MASK = ALIGNMENT - 1;

        if (value > static_cast<std::size_t>(-1) - MASK)
        {
            return false;
        }

        result = (value + MASK) & ~MASK;
        return true;
    }
}

void memory_init()
{
    heap_position = 0;

    for (std::size_t i = 0; i < MAX_ALLOCATIONS; ++i)
    {
        allocations[i].address = nullptr;
        allocations[i].size = 0;
        allocations[i].free = false;
    }
}

void* kmalloc(std::size_t size)
{
    if (size == 0)
    {
        return nullptr;
    }

    // Reuse a freed allocation when it is large enough.
    for (std::size_t i = 0; i < MAX_ALLOCATIONS; ++i)
    {
        if (allocations[i].free &&
            allocations[i].size >= size)
        {
            allocations[i].free = false;
            return allocations[i].address;
        }
    }

    // Find an unused allocation record.
    std::size_t allocationIndex = MAX_ALLOCATIONS;

    for (std::size_t i = 0; i < MAX_ALLOCATIONS; ++i)
    {
        if (allocations[i].address == nullptr)
        {
            allocationIndex = i;
            break;
        }
    }

    if (allocationIndex == MAX_ALLOCATIONS)
    {
        return nullptr;
    }

    // Round the allocation size up to a 16-byte boundary.
    std::size_t aligned_size = 0;

    if (!align_up(size, aligned_size))
    {
        return nullptr;
    }

    // Check available heap space without overflowing.
    if (heap_position > HEAP_SIZE ||
        aligned_size > HEAP_SIZE - heap_position)
    {
        return nullptr;
    }

    void* address =
        reinterpret_cast<void*>(HEAP_START + heap_position);

    heap_position += aligned_size;

    allocations[allocationIndex].address = address;
    allocations[allocationIndex].size = aligned_size;
    allocations[allocationIndex].free = false;

    return address;
}

void kfree(void* ptr)
{
    if (ptr == nullptr)
    {
        return;
    }

    for (std::size_t i = 0; i < MAX_ALLOCATIONS; ++i)
    {
        if (allocations[i].address == ptr &&
            !allocations[i].free)
        {
            allocations[i].free = true;
            return;
        }
    }
}
