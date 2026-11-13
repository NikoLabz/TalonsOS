#include "memory.h"

#include <new>

// Phase 5 M3 changes:
//   * operator new never returns nullptr. A failed allocation goes to
//     memory_fatal_out_of_memory() (halts unless a handler is installed),
//     because callers of plain `new` are entitled to assume success and
//     the kernel has no exceptions. Previously nullptr was returned.
//   * operator new(0) returns a valid unique pointer (kmalloc(0) is
//     nullptr), as the language requires.
//   * std::nothrow forms were added; they return nullptr on failure.
// Nothing in the kernel used plain `new` at the time of this change
// (only placement new), so no existing behaviour depended on it.

namespace
{
    void* allocateOrDie(std::size_t size)
    {
        void* pointer = kmalloc(size != 0 ? size : 1);

        if (pointer == nullptr)
        {
            memory_fatal_out_of_memory(size);
            // A notification handler may return, but throwing operator new
            // must never hand the caller a null or invalid pointer.
            memory_halt_out_of_memory(size);
        }

        return pointer;
    }
}

void* operator new(std::size_t size)
{
    return allocateOrDie(size);
}

void* operator new[](std::size_t size)
{
    return allocateOrDie(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    return kmalloc(size != 0 ? size : 1);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    return kmalloc(size != 0 ? size : 1);
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

void operator delete(void* ptr, const std::nothrow_t&) noexcept
{
    kfree(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
    kfree(ptr);
}

// Deliberate shell-only runtime probe. The request is guaranteed to exceed
// the fixed heap, then the normal non-allocating panic path must halt.
extern "C" [[noreturn]] void memory_test_trigger_oom()
{
    const std::size_t request = memory_max_allocation() + 1;
    (void)::operator new(request);
    memory_halt_out_of_memory(request);
}
