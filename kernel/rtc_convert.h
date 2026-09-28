#pragma once

// ============================================================
// Pure RTC field conversion.
//
// Split out of kernel/kernel_time.cpp so the arithmetic can be
// exercised by a host test (filesystem/talonsfs/src/test_rtc.cpp)
// without needing a CMOS chip. Nothing in here touches hardware or
// the C++ runtime, so it is safe to include from freestanding code.
// ============================================================

#include <stdint.h>

namespace talons_rtc
{

// Binary-coded-decimal byte -> binary. 0x59 -> 59.
inline uint8_t fromBcd(uint8_t value)
{
    return static_cast<uint8_t>((value & 0x0F) + ((value >> 4) * 10));
}

/*
 * Days since 1970-01-01 from a proleptic Gregorian date.
 *
 * Howard Hinnant's days_from_civil: shifting the year to start in
 * March makes the leap day the final day of the year, which removes
 * every special case from the month-length arithmetic.
 */
inline int64_t daysFromCivil(int64_t year, uint32_t month, uint32_t day)
{
    year -= (month <= 2) ? 1 : 0;

    const int64_t era = (year >= 0 ? year : year - 399) / 400;

    const uint32_t yoe =
        static_cast<uint32_t>(year - era * 400);               // 0..399

    const uint32_t doy =
        (153u * (month + (month > 2 ? -3u : 9u)) + 2u) / 5u
        + day - 1u;                                             // 0..365

    const uint32_t doe =
        yoe * 365u + yoe / 4u - yoe / 100u + doy;               // 0..146096

    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

// Returns 0 for out-of-range input, matching the "no timestamp"
// convention TalonsFS uses when the clock is unusable.
inline uint64_t toUnixTime(int64_t year,
                           uint32_t month,
                           uint32_t day,
                           uint32_t hour,
                           uint32_t minute,
                           uint32_t second)
{
    if (month < 1 || month > 12 ||
        day   < 1 || day   > 31 ||
        hour  > 23 || minute > 59 || second > 60)
    {
        return 0;
    }

    const int64_t seconds =
        daysFromCivil(year, month, day) * 86400
        + static_cast<int64_t>(hour) * 3600
        + static_cast<int64_t>(minute) * 60
        + static_cast<int64_t>(second);

    if (seconds < 0)
    {
        return 0;
    }

    return static_cast<uint64_t>(seconds);
}

/*
 * Convert a raw 12-hour-mode hour register to a 24-hour value.
 * Bit 7 is the PM flag and MUST be stripped before BCD decoding,
 * otherwise it corrupts the high nibble.
 */
inline uint8_t decodeHour(uint8_t rawHour, bool hour24, bool binaryMode)
{
    bool pm = false;
    uint8_t hour = rawHour;

    if (!hour24)
    {
        pm = (hour & 0x80) != 0;
        hour = static_cast<uint8_t>(hour & 0x7F);
    }

    if (!binaryMode)
    {
        hour = fromBcd(hour);
    }

    if (!hour24)
    {
        if (hour == 12)
        {
            hour = 0;
        }

        if (pm)
        {
            hour = static_cast<uint8_t>(hour + 12);
        }
    }

    return hour;
}

}
