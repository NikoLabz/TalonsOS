// ============================================================
// Kernel-side wall-clock time for TalonsFS.
//
// TalonsFS::currentUnixTime() has two implementations:
//
//   filesystem/talonsfs/src/time.cpp   host tools and host tests
//                                      (uses <ctime>)
//   kernel/kernel_time.cpp             this file -- the kernel,
//                                      which reads the CMOS RTC
//
// Exactly ONE is linked into any given binary: the Makefile keeps
// `time` out of TALONSFS_SRCS and puts `kernel_time` in KERNEL_SRCS.
// Linking both is a duplicate-symbol error, by design.
//
// The RTC sits behind the CMOS index/data port pair:
//
//   0x70   index register (bit 7 = NMI disable -- leave clear)
//   0x71   data register
//
// Registers used:
//
//   0x00 seconds   0x02 minutes   0x04 hours
//   0x07 day       0x08 month     0x09 year (two digits)
//   0x0A status A  bit 7 = update in progress
//   0x0B status B  bit 2 = binary (else BCD), bit 1 = 24-hour
//
// There is no century register that can be relied upon across
// machines (0x32 is common but not universal), so the two-digit year
// maps to 2000..2099. TalonsOS need not survive the year 2100.
//
// The date arithmetic lives in kernel/rtc_convert.h so it can be
// unit-tested on the host without a CMOS chip; see
// filesystem/talonsfs/src/test_rtc.cpp.
// ============================================================

#include <stdint.h>

#include "rtc_convert.h"
#include "talonsfs/time.h"

namespace
{
    constexpr uint16_t CMOS_INDEX = 0x70;
    constexpr uint16_t CMOS_DATA  = 0x71;

    inline void outb(uint16_t port, uint8_t value)
    {
        asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
    }

    inline uint8_t inb(uint16_t port)
    {
        uint8_t value;
        asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
        return value;
    }

    uint8_t cmosRead(uint8_t reg)
    {
        outb(CMOS_INDEX, reg);
        return inb(CMOS_DATA);
    }

    bool updateInProgress()
    {
        return (cmosRead(0x0A) & 0x80) != 0;
    }

    struct RtcTime
    {
        uint8_t second;
        uint8_t minute;
        uint8_t hour;
        uint8_t day;
        uint8_t month;
        uint8_t year;
    };

    RtcTime readRtcRaw()
    {
        RtcTime t{};

        t.second = cmosRead(0x00);
        t.minute = cmosRead(0x02);
        t.hour   = cmosRead(0x04);
        t.day    = cmosRead(0x07);
        t.month  = cmosRead(0x08);
        t.year   = cmosRead(0x09);

        return t;
    }

    bool sameTime(const RtcTime& a, const RtcTime& b)
    {
        return a.second == b.second &&
               a.minute == b.minute &&
               a.hour   == b.hour   &&
               a.day    == b.day    &&
               a.month  == b.month  &&
               a.year   == b.year;
    }
}

namespace TalonsFS {

uint64_t currentUnixTime()
{
    /*
     * Read twice around the update-in-progress flag and require two
     * identical results. A single read can straddle an RTC update and
     * return a mix of old and new fields (23:59:59 -> 23:00:00).
     *
     * Every loop is bounded, so a missing or wedged RTC degrades to
     * "no timestamp" (0) rather than hanging the kernel.
     */
    RtcTime first{};
    RtcTime second{};

    bool stable = false;

    for (uint32_t attempt = 0; attempt < 128; ++attempt)
    {
        uint32_t guard = 0;

        while (updateInProgress() && guard < 1000000u)
        {
            ++guard;
        }

        first = readRtcRaw();

        while (updateInProgress() && guard < 2000000u)
        {
            ++guard;
        }

        second = readRtcRaw();

        if (sameTime(first, second))
        {
            stable = true;
            break;
        }
    }

    if (!stable)
    {
        return 0;
    }

    const uint8_t statusB = cmosRead(0x0B);

    const bool binaryMode = (statusB & 0x04) != 0;
    const bool hour24     = (statusB & 0x02) != 0;

    const uint8_t hour =
        talons_rtc::decodeHour(first.hour, hour24, binaryMode);

    uint8_t sec   = first.second;
    uint8_t min   = first.minute;
    uint8_t day   = first.day;
    uint8_t month = first.month;
    uint8_t year  = first.year;

    if (!binaryMode)
    {
        sec   = talons_rtc::fromBcd(sec);
        min   = talons_rtc::fromBcd(min);
        day   = talons_rtc::fromBcd(day);
        month = talons_rtc::fromBcd(month);
        year  = talons_rtc::fromBcd(year);
    }

    return talons_rtc::toUnixTime(
        2000 + static_cast<int64_t>(year),
        month,
        day,
        hour,
        min,
        sec
    );
}

}
