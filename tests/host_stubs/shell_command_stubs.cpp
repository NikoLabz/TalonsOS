// Host-only stubs for shell commands not exercised by the navigation
// regression test. Never linked into the real kernel or QEMU build.

#include <cstdlib>

void talonc_run(const char*, int, int)
{
}

void talonedit_run(const char*)
{
}

// Phase 5 M1: shell memory reports are not exercised by the navigation
// regression test (they are covered by kernel/test_meminfo.cpp).
void meminfo_run(int, int)
{
}

void memmap_run(int, int)
{
}

void memcheck_run()
{
}

// The navigation test links shell.cpp but never invokes the destructive
// page-fault diagnostic command.
extern "C" [[noreturn]] void exception_test_trigger_page_fault()
{
    __builtin_trap();
}

extern "C" [[noreturn]] void memory_test_trigger_oom()
{
    std::abort();
}
