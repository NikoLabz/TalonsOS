#include "memory.h"

void* operator new(std::size_t size)
{
    return kmalloc(size);
}

void* operator new[](std::size_t size)
{
    return kmalloc(size);
}

void operator delete(void* ptr) noexcept
{
    kfree(ptr);
}

void operator delete[](void* ptr) noexcept
{
    kfree(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
    kfree(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
    kfree(ptr);
}
