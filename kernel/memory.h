#pragma once

#include "heap.h"

#include <cstddef>

// Kernel heap API.
//
// Existing functions (signatures unchanged since before Phase 5):
//   memory_init()  - (re)initialises the heap; every earlier pointer
//                    becomes invalid. Calling kmalloc()/kfree() before
//                    memory_init() also works: the heap initialises
//                    itself on first use.
//   kmalloc(size)  - 16-byte aligned block of at least 'size' bytes;
//                    nullptr for size 0 or when no memory is available.
//   kfree(ptr)     - nullptr is ignored; pointers that are not live
//                    allocations (including double frees) are detected,
//                    counted and ignored.
//
// Allocator context restriction: kmalloc()/kfree() are not interrupt-safe
// and must not be called from CPU exception or interrupt handlers. Exception
// diagnostics use fixed storage and direct VGA writes instead.
void memory_init();

void* kmalloc(std::size_t size);
void kfree(void* ptr);

// ---- Phase 5 M3 additions ----

// Heap statistics. Returns false if the heap walk hit a corrupted header
// (the fields gathered so far are still filled in).
bool memory_get_stats(talons_heap::Stats& out);

// Full read-only integrity check (headers, canaries, free-block poison,
// merge invariant). Returns report.ok.
bool memory_check(talons_heap::CheckReport& out);

// Largest single request an empty heap can satisfy.
std::size_t memory_max_allocation();

// Optional notification of heap errors (out of memory, double free,
// corruption ...). The handler must not call kmalloc()/kfree().
void memory_set_error_handler(talons_heap::ErrorHandler handler);

// Out-of-memory policy for operator new (which must not return nullptr).
// memory_fatal_out_of_memory() calls the registered handler and returns if
// one is set; with no handler it reports "OUT OF MEMORY" and halts the CPU
// (it does not return). kmalloc() itself never calls this.
void memory_set_fatal_handler(void (*handler)(std::size_t requested));
void memory_fatal_out_of_memory(std::size_t requested);

// Non-returning final policy used when a throwing allocation cannot recover,
// including when a registered notification handler returns.
[[noreturn]] void memory_halt_out_of_memory(std::size_t requested);
