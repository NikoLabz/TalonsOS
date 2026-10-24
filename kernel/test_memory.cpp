#include "memory.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

int main()
{
    memory_init();

    // 1. Zero-sized requests must fail.
    assert(kmalloc(0) == nullptr);

    // 2. New allocations must be 16-byte aligned.
    void* aligned = kmalloc(1);
    assert(aligned != nullptr);
    assert(reinterpret_cast<std::uintptr_t>(aligned) % 16 == 0);

    void* aligned_again = kmalloc(1);
    assert(aligned_again != nullptr);
    assert(reinterpret_cast<std::uintptr_t>(aligned_again) % 16 == 0);

    // 3. Basic allocation and reuse.
    memory_init();

    void* first = kmalloc(128);
    assert(first != nullptr);
    kfree(first);

    void* reused = kmalloc(64);
    assert(reused == first);

    void* second = kmalloc(64);
    assert(second != nullptr);
    assert(second != reused);

    // 4. Invalid and duplicate frees must be harmless.
    int unrelated = 0;
    kfree(&unrelated);
    kfree(reused);
    kfree(reused);

    // 5. Freed blocks should remain reusable.
    void* reused_again = kmalloc(32);
    assert(reused_again == reused);

    // 6. Exhaust the allocation records.
    memory_init();

    std::size_t successful = 0;

    for (std::size_t i = 0; i < 300; ++i)
    {
        if (kmalloc(1) != nullptr)
        {
            ++successful;
        }
    }

    assert(successful == 256);
    assert(kmalloc(1) == nullptr);

    // 7. Reinitialisation must reset allocator state.
    memory_init();
    assert(kmalloc(128) != nullptr);

    // 8. Heap exhaustion must fail safely.
    memory_init();
    void* entire_heap = kmalloc(1024 * 1024);
    assert(entire_heap != nullptr);
    assert(kmalloc(1) == nullptr);

    // A failed allocation must not corrupt the existing allocation.
    assert(entire_heap != nullptr);
    kfree(entire_heap);

    // The heap can reuse a freed allocation.
    assert(kmalloc(1024) == entire_heap);

    std::puts("Memory allocator regression tests passed.");
    return 0;
}
