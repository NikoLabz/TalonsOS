#include "memory.h"

#include "heap.h"

namespace
{
    // Kernel heap region: 1 MiB at 0x100000, inside the boot identity map
    // and reserved from the frame allocator by kernel_main().
    constexpr std::size_t HEAP_START = 0x100000;
    constexpr std::size_t HEAP_SIZE = 0x100000;

#ifdef TALONS_HOST_TEST
    // Host unit tests cannot use a fixed physical address.
    alignas(16) unsigned char hostHeap[HEAP_SIZE];

    void* heapBase()
    {
        return hostHeap;
    }
#else
    void* heapBase()
    {
        return reinterpret_cast<void*>(HEAP_START);
    }
#endif

    // Zero-initialised in .bss; valid (uninitialised) before memory_init().
    talons_heap::Heap heap;

    void (*fatalHandler)(std::size_t) = nullptr;

    bool ensureInitialised()
    {
        if (heap.isInitialised())
            return true;

        return heap.init(heapBase(), HEAP_SIZE);
    }
}

void memory_init()
{
    heap.init(heapBase(), HEAP_SIZE);
}

void* kmalloc(std::size_t size)
{
    if (!ensureInitialised())
        return nullptr;

    return heap.allocate(size);
}

void kfree(void* ptr)
{
    if (ptr == nullptr || !ensureInitialised())
        return;

    heap.free(ptr);
}

bool memory_get_stats(talons_heap::Stats& out)
{
    out = talons_heap::Stats{};
    if (!heap.isInitialised())
    {
        return false;
    }

    return heap.getStats(out);
}

bool memory_check(talons_heap::CheckReport& out)
{
    out = talons_heap::CheckReport{};
    if (!heap.isInitialised())
    {
        out.truncated = true;
        return false;
    }

    return heap.check(out);
}

std::size_t memory_max_allocation()
{
    return ensureInitialised() ? heap.maxAllocation() : 0;
}

void memory_set_error_handler(talons_heap::ErrorHandler handler)
{
    heap.setErrorHandler(handler);
}

void memory_set_fatal_handler(void (*handler)(std::size_t))
{
    fatalHandler = handler;
}

[[noreturn]] void memory_halt_out_of_memory(std::size_t requested)
{
    (void)requested;
#ifdef TALONS_HOST_TEST
    __builtin_trap();
#else
    // Fixed direct VGA output: safe before heap initialisation and during
    // allocator failure. Never route this through streams or formatting.
    volatile unsigned short* screen =
        reinterpret_cast<volatile unsigned short*>(0xB8000);
    const char message[] = "KERNEL PANIC: OUT OF MEMORY";
    for (std::size_t i = 0; message[i] != '\0'; ++i)
        screen[24 * 80 + i] =
            static_cast<unsigned short>(0x4F00 | message[i]);
    for (;;)
        asm volatile("cli; hlt");
#endif
}

void memory_fatal_out_of_memory(std::size_t requested)
{
    // Re-entry guard: a fatal handler (or anything it calls) that runs out
    // of memory again must not recurse into the handler. Single CPU and
    // interrupts disabled, so a plain flag is sufficient.
    static bool active = false;

    if (active)
        memory_halt_out_of_memory(requested);

    active = true;

    if (fatalHandler != nullptr)
    {
        fatalHandler(requested);
        active = false;
        return;
    }

#ifdef TALONS_HOST_TEST
    memory_halt_out_of_memory(requested);
#else
    memory_halt_out_of_memory(requested);
#endif
}
