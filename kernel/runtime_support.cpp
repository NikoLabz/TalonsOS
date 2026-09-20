// ============================================================
// Minimal freestanding C/C++ runtime support.
//
// Two things in this kernel now require symbols that a hosted
// libc/libstdc++ would normally provide, and -ffreestanding
// deliberately gives us neither:
//
//   1. memcpy/memset/memmove/memcmp: GCC's __builtin_memcpy (used
//      throughout the TalonsFS integration for buffers whose size
//      isn't a compile-time constant) lowers to a call to the real
//      library function once it can't be inlined.
//
//   2. __cxa_atexit / __dso_handle: KernelBlockDevice now inherits
//      TalonsFS::BlockDevice (virtual destructor), so its global
//      instance has a non-trivial destructor. The Itanium C++ ABI
//      registers such destructors via __cxa_atexit at static-init
//      time. The kernel never exits, so registration is a no-op.
// ============================================================

#include <stddef.h>

extern "C"
{

void* memcpy(void* dest, const void* src, size_t n)
{
    unsigned char* d = static_cast<unsigned char*>(dest);
    const unsigned char* s = static_cast<const unsigned char*>(src);

    for (size_t i = 0; i < n; ++i)
    {
        d[i] = s[i];
    }

    return dest;
}

void* memset(void* dest, int value, size_t n)
{
    unsigned char* d = static_cast<unsigned char*>(dest);

    for (size_t i = 0; i < n; ++i)
    {
        d[i] = static_cast<unsigned char>(value);
    }

    return dest;
}

void* memmove(void* dest, const void* src, size_t n)
{
    unsigned char* d = static_cast<unsigned char*>(dest);
    const unsigned char* s = static_cast<const unsigned char*>(src);

    if (d < s)
    {
        for (size_t i = 0; i < n; ++i)
        {
            d[i] = s[i];
        }
    }
    else
    {
        for (size_t i = n; i > 0; --i)
        {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

int memcmp(const void* a, const void* b, size_t n)
{
    const unsigned char* pa = static_cast<const unsigned char*>(a);
    const unsigned char* pb = static_cast<const unsigned char*>(b);

    for (size_t i = 0; i < n; ++i)
    {
        if (pa[i] != pb[i])
        {
            return static_cast<int>(pa[i]) - static_cast<int>(pb[i]);
        }
    }

    return 0;
}

void* __dso_handle = nullptr;

int __cxa_atexit(void (*)(void*), void*, void*)
{
    // The kernel never exits; nothing to register.
    return 0;
}

void __cxa_pure_virtual()
{
    // A pure virtual call this deep into a hobby kernel means a
    // real bug. Halt instead of returning into undefined behaviour.
    for (;;)
    {
        asm volatile ("hlt");
    }
}

} // extern "C"
