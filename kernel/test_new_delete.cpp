// Phase 5 M3: tests for the real kernel/new_delete.cpp operators on top of
// the real kernel/memory.cpp + heap.cpp (host build, static heap buffer).
// The operators in this test program ARE the kernel's: new_delete.cpp
// replaces the global operator new/delete here.

#include "memory.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <csignal>
#include <new>

#include <sys/wait.h>
#include <unistd.h>

namespace
{
    std::size_t fatalCalls = 0;
    std::size_t fatalSize = 0;
    int fatalNotificationFd = -1;

    void onFatal(std::size_t requested)
    {
        ++fatalCalls;
        fatalSize = requested;
    }

    int constructed = 0;
    int destroyed = 0;
    void* opaque(void* p);

    struct Tracked
    {
        int value;
        Tracked() : value(7) { ++constructed; }
        ~Tracked() { ++destroyed; }
    };

    // Larger than the whole heap: allocation can never succeed.
    struct Huge
    {
        char bytes[2 * 1024 * 1024];
        Huge() { ++constructed; }
    };

    // Runs 'body' in a child process; returns the signal that killed it
    // (0 if it exited normally).
    template <typename Body>
    int deathSignalOf(Body body)
    {
        const pid_t child = fork();
        assert(child >= 0);

        if (child == 0)
        {
            body();
            _exit(0);
        }

        int status = 0;
        assert(waitpid(child, &status, 0) == child);
        return WIFSIGNALED(status) ? WTERMSIG(status) : 0;
    }

    void returningFatalHandler(std::size_t requested)
    {
        if (fatalNotificationFd >= 0)
            (void)write(fatalNotificationFd, &requested, sizeof(requested));
    }

    int throwingOomOutcome(std::size_t requested, bool array,
                           bool& handlerCalled, std::size_t& reported)
    {
        int descriptors[2];
        assert(pipe(descriptors) == 0);
        const pid_t child = fork();
        assert(child >= 0);
        if (child == 0)
        {
            close(descriptors[0]);
            fatalNotificationFd = descriptors[1];
            memory_set_fatal_handler(returningFatalHandler);
            if (array)
                (void)opaque(::operator new[](requested));
            else
                (void)opaque(::operator new(requested));
            _exit(0); // Returning from throwing new is a test failure.
        }

        close(descriptors[1]);
        reported = 0;
        const ssize_t count = read(descriptors[0], &reported, sizeof(reported));
        close(descriptors[0]);
        int status = 0;
        assert(waitpid(child, &status, 0) == child);
        handlerCalled = count == static_cast<ssize_t>(sizeof(reported));
        return WIFSIGNALED(status) ? WTERMSIG(status) : 0;
    }

    // Stops the compiler from assuming operator new never returns nullptr.
    void* opaque(void* p)
    {
        asm volatile ("" : "+r"(p));
        return p;
    }
}

int main()
{
    memory_init();
    memory_set_fatal_handler(onFatal);

    talons_heap::Stats before;
    assert(memory_get_stats(before));

    // Plain new/delete round trip.
    int* value = new int(42);
    assert(*value == 42);
    assert(reinterpret_cast<std::uintptr_t>(value) % 16 == 0);
    delete value;

    // Arrays, including sized delete.
    char* array = new char[1000];
    array[0] = 1;
    array[999] = 2;
    ::operator delete[](array, std::size_t(1000));

    // operator new(0) must return a valid, unique pointer (kmalloc(0) is
    // nullptr, so the operator asks for one byte).
    void* z1 = ::operator new(0);
    void* z2 = ::operator new(0);
    assert(z1 != nullptr && z2 != nullptr && z1 != z2);
    ::operator delete(z1);
    ::operator delete(z2, std::size_t(0));

    int* empty = new int[0];
    assert(empty != nullptr);
    delete[] empty;

    // nothrow forms behave the same on success.
    int* quiet = new (std::nothrow) int(7);
    assert(quiet != nullptr && *quiet == 7);
    ::operator delete(quiet, std::nothrow);

    // Nothing leaked: every block is back.
    talons_heap::Stats after;
    assert(memory_get_stats(after));
    assert(after.allocatedBlocks == before.allocatedBlocks);
    assert(after.bytesInUse == before.bytesInUse);
    assert(fatalCalls == 0);

    // ---- out of memory ----
    const std::size_t tooBig = memory_max_allocation() + 1;

    // nothrow: returns nullptr and does NOT trigger the fatal path.
    void* none = opaque(::operator new(tooBig, std::nothrow));
    assert(none == nullptr);
    void* noneArray = opaque(::operator new[](tooBig, std::nothrow));
    assert(noneArray == nullptr);
    assert(fatalCalls == 0);

    // Throwing forms must halt even when the notification handler returns;
    // they may never leak nullptr to code that assumes allocation succeeded.
    bool handlerCalled = false;
    std::size_t reported = 0;
    assert(throwingOomOutcome(tooBig, false, handlerCalled, reported) == SIGILL);
    assert(handlerCalled && reported == tooBig);
    assert(throwingOomOutcome(tooBig + 7, true,
                             handlerCalled, reported) == SIGILL);
    assert(handlerCalled && reported == tooBig + 7);

    // The heap is unharmed and still working.
    void* ok = ::operator new(128);
    assert(ok != nullptr);
    ::operator delete(ok);

    talons_heap::CheckReport report;
    assert(memory_check(report));

    talons_heap::Stats end;
    assert(memory_get_stats(end));
    assert(end.outOfMemoryEvents == before.outOfMemoryEvents + 2);

    // ---- review items: every form of new, in detail ----

    memory_init();
    talons_heap::Stats base;
    assert(memory_get_stats(base));

    // Placement new never touches the heap: it just returns the address
    // (operator new(size_t, void*) is inline in <new>, not ours).
    alignas(Tracked) unsigned char slot[sizeof(Tracked)];
    constructed = destroyed = 0;
    Tracked* placed = new (slot) Tracked;
    assert(static_cast<void*>(placed) == static_cast<void*>(slot));
    assert(placed->value == 7 && constructed == 1);
    placed->~Tracked();
    assert(destroyed == 1);

    alignas(Tracked) unsigned char slots[3 * sizeof(Tracked)];
    constructed = destroyed = 0;
    Tracked* placedArray = new (slots) Tracked[3];
    assert(static_cast<void*>(placedArray) == static_cast<void*>(slots));
    assert(constructed == 3);
    for (int i = 0; i < 3; ++i)
        placedArray[i].~Tracked();
    assert(destroyed == 3);

    talons_heap::Stats afterPlacement;
    assert(memory_get_stats(afterPlacement));
    assert(afterPlacement.totalAllocations == base.totalAllocations);
    assert(afterPlacement.totalFrees == base.totalFrees);
    assert(afterPlacement.outOfMemoryEvents == base.outOfMemoryEvents);

    // Ordinary new runs the constructor, delete runs the destructor.
    constructed = destroyed = 0;
    Tracked* ordinary = new Tracked;
    assert(constructed == 1 && ordinary->value == 7);
    delete ordinary;
    assert(destroyed == 1);

    constructed = destroyed = 0;
    Tracked* many = new Tracked[5];
    assert(constructed == 5);
    delete[] many;
    assert(destroyed == 5);

    // delete of a null pointer is a harmless no-op (no error counted).
    Tracked* none2 = nullptr;
    delete none2;
    delete[] static_cast<Tracked*>(nullptr);
    ::operator delete(nullptr, std::nothrow);

    // nothrow new that fails returns nullptr and, as the language
    // requires, does NOT run the constructor and does NOT enter the fatal
    // path.
    fatalCalls = 0;
    constructed = 0;
    Huge* failed = opaque(new (std::nothrow) Huge) == nullptr
                       ? nullptr
                       : reinterpret_cast<Huge*>(1);
    assert(failed == nullptr);
    assert(constructed == 0);
    assert(fatalCalls == 0);

    // Array-length overflow: the compiler passes SIZE_MAX on overflow; it
    // must fail cleanly into the fatal path, not wrap around.
    bool overflowHandlerCalled = false;
    std::size_t overflowReported = 0;
    assert(throwingOomOutcome(SIZE_MAX, true, overflowHandlerCalled,
                              overflowReported) == SIGILL);
    assert(overflowHandlerCalled && overflowReported == SIZE_MAX);

    // Deleting a pointer the heap does not own is detected, counted and
    // ignored (never crashes).
    int stackObject = 0;
    talons_heap::Stats beforeBad;
    assert(memory_get_stats(beforeBad));
    ::operator delete(opaque(&stackObject));
    talons_heap::Stats afterBad;
    assert(memory_get_stats(afterBad));
    assert(afterBad.invalidFrees == beforeBad.invalidFrees + 1);
    memory_set_fatal_handler(nullptr);

    // Default OOM policy (no handler installed) must not return: on the
    // host build it traps, in the kernel it halts. Run in a child process.
    assert(deathSignalOf([] {
        void* p = opaque(::operator new(memory_max_allocation() + 1));
        (void)p;
    }) == SIGILL);

    // A fatal handler that itself runs out of memory must not recurse
    // into the handler; the guard stops the CPU instead.
    assert(deathSignalOf([] {
        memory_set_fatal_handler([](std::size_t) {
            void* p = opaque(::operator new(memory_max_allocation() + 1));
            (void)p;
        });
        void* p = opaque(::operator new(memory_max_allocation() + 1));
        (void)p;
    }) == SIGILL);

    // The notification API can return normally and clears its re-entry guard.
    memory_set_fatal_handler(onFatal);
    fatalCalls = 0;
    memory_fatal_out_of_memory(memory_max_allocation() + 1);
    memory_fatal_out_of_memory(memory_max_allocation() + 1);
    assert(fatalCalls == 2);

    memory_set_fatal_handler(nullptr);
    std::puts("operator new/delete tests passed.");
    return 0;
}
