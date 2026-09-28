// ============================================================
// HOST-ONLY wall-clock time for TalonsFS.
//
// This file is linked into the host test suite and into
// tools/mkTalonsFS. It is deliberately NOT part of the kernel build:
// the kernel gets kernel/kernel_time.cpp, which reads the CMOS RTC.
//
// Exactly one of the two must be linked into any given binary. See
// TALONSFS_SRCS / KERNEL_SRCS in the Makefile.
// ============================================================

#include <cstdint>
#include <ctime>

#include "time.h"

namespace TalonsFS {

uint64_t currentUnixTime()
{
    const std::time_t now = std::time(nullptr);

    if (now == static_cast<std::time_t>(-1))
    {
        return 0;
    }

    return static_cast<uint64_t>(now);
}

}
