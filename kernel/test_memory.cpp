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

    // 6. DELIBERATELY CHANGED in Phase 5 M3 (approved heap redesign).
    //
    // The old allocator kept at most 256 allocation records, so exactly
    // 256 of 300 kmalloc(1) calls succeeded and the 257th failed even
    // though the heap was nearly empty. That artificial limit is gone:
    // allocation is now limited only by the heap size. Every one of the
    // first 300 calls must succeed, and the heap must still fail safely
    // (and recover) when it is genuinely full.
    memory_init();

    {
        static void* blocks[40000];
        std::size_t successful = 0;

        for (std::size_t i = 0; i < 300; ++i)
        {
            blocks[i] = kmalloc(1);
            assert(blocks[i] != nullptr);
            ++successful;
        }

        assert(successful == 300);

        // Keep going until the 1 MiB heap is really full.
        while (successful < 40000)
        {
            void* block = kmalloc(1);
            if (block == nullptr)
                break;
            blocks[successful++] = block;
        }

        assert(successful > 20000);      // ~48 bytes per kmalloc(1)
        assert(successful < 40000);      // it did fill up
        assert(kmalloc(1) == nullptr);   // full: safe failure

        for (std::size_t i = 0; i < successful; ++i)
            kfree(blocks[i]);

        // Everything freed: the whole heap is available again.
        void* everything = kmalloc(memory_max_allocation());
        assert(everything != nullptr);
        kfree(everything);
    }

    // 7. Reinitialisation must reset allocator state.
    memory_init();
    assert(kmalloc(128) != nullptr);

    // 8. Heap exhaustion must fail safely.
    //
    // DELIBERATELY CHANGED in Phase 5 M3: block headers now live inside
    // the heap, so a request for the full 1 MiB can no longer succeed.
    // The largest possible single allocation is memory_max_allocation().
    memory_init();
    assert(kmalloc(1024 * 1024) == nullptr);
    assert(kmalloc(memory_max_allocation() + 1) == nullptr);
    void* entire_heap = kmalloc(memory_max_allocation());
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
