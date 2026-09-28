// ============================================================
// Host test for kernel/rtc_convert.h.
//
// The kernel's CMOS reader cannot run here (no port I/O), but the
// arithmetic it depends on is pure and IS the code the kernel links.
// Validate it against the host's own timegm() over a wide date range,
// plus the BCD and 12-hour cases that are easy to get wrong.
// ============================================================

#include "rtc_convert.h"

#include <ctime>
#include <cstdio>
#include <cstdint>

namespace
{
    int failures = 0;

    void check(bool condition, const char* what)
    {
        std::printf("%s: %s\n", what, condition ? "PASSED" : "FAILED");
        if (!condition)
        {
            ++failures;
        }
    }

    uint64_t reference(int year, int month, int day,
                       int hour, int minute, int second)
    {
        std::tm tm{};
        tm.tm_year = year - 1900;
        tm.tm_mon  = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min  = minute;
        tm.tm_sec  = second;

        return static_cast<uint64_t>(timegm(&tm));
    }
}

int main()
{
    // ---- Known fixed points -------------------------------------
    check(talons_rtc::toUnixTime(1970, 1, 1, 0, 0, 0) == 0,
          "Unix epoch maps to 0");

    check(talons_rtc::toUnixTime(2000, 1, 1, 0, 0, 0) == 946684800ull,
          "2000-01-01 maps to 946684800");

    check(talons_rtc::toUnixTime(2024, 2, 29, 12, 0, 0)
              == reference(2024, 2, 29, 12, 0, 0),
          "Leap day 2024-02-29 matches timegm");

    // ---- Exhaustive sweep against timegm ------------------------
    bool sweepOk = true;
    long compared = 0;

    for (int year = 2000; year <= 2099 && sweepOk; ++year)
    {
        for (int month = 1; month <= 12 && sweepOk; ++month)
        {
            for (int day = 1; day <= 28; ++day)
            {
                const uint64_t got =
                    talons_rtc::toUnixTime(year, month, day, 13, 37, 42);

                const uint64_t want =
                    reference(year, month, day, 13, 37, 42);

                ++compared;

                if (got != want)
                {
                    std::printf("  mismatch %04d-%02d-%02d got=%llu want=%llu\n",
                                year, month, day,
                                (unsigned long long)got,
                                (unsigned long long)want);
                    sweepOk = false;
                    break;
                }
            }
        }
    }

    std::printf("  (compared %ld dates across 2000-2099)\n", compared);
    check(sweepOk, "Full 2000-2099 sweep matches timegm");

    // ---- Month-end and year-end boundaries ----------------------
    bool boundaryOk = true;
    const int ends[][3] = {
        {2024, 1, 31}, {2024, 2, 29}, {2023, 2, 28}, {2024, 4, 30},
        {2024, 12, 31}, {2000, 2, 29}, {2100, 1, 1}
    };

    for (const auto& e : ends)
    {
        if (talons_rtc::toUnixTime(e[0], e[1], e[2], 23, 59, 59)
            != reference(e[0], e[1], e[2], 23, 59, 59))
        {
            boundaryOk = false;
            std::printf("  boundary mismatch %04d-%02d-%02d\n",
                        e[0], e[1], e[2]);
        }
    }
    check(boundaryOk, "Month-end and leap-year boundaries correct");

    // ---- BCD decoding -------------------------------------------
    check(talons_rtc::fromBcd(0x00) == 0 &&
          talons_rtc::fromBcd(0x09) == 9 &&
          talons_rtc::fromBcd(0x10) == 10 &&
          talons_rtc::fromBcd(0x59) == 59 &&
          talons_rtc::fromBcd(0x99) == 99,
          "BCD decoding correct");

    // ---- 12/24-hour decoding ------------------------------------
    // 24-hour binary: value passes straight through.
    check(talons_rtc::decodeHour(23, true, true) == 23,
          "24-hour binary hour passes through");

    // 24-hour BCD: 0x23 -> 23.
    check(talons_rtc::decodeHour(0x23, true, false) == 23,
          "24-hour BCD hour decodes");

    // 12-hour BCD, 12 AM -> 0.
    check(talons_rtc::decodeHour(0x12, false, false) == 0,
          "12 AM maps to hour 0");

    // 12-hour BCD, 12 PM -> 12 (0x80 PM flag set).
    check(talons_rtc::decodeHour(0x92, false, false) == 12,
          "12 PM maps to hour 12");

    // 12-hour BCD, 1 PM -> 13. Raw 0x81: PM flag + BCD 1.
    // This is the case that breaks if the PM bit is stripped AFTER
    // the BCD conversion instead of before it.
    check(talons_rtc::decodeHour(0x81, false, false) == 13,
          "1 PM maps to hour 13 (PM bit stripped before BCD)");

    // 12-hour BCD, 11 PM -> 23.
    check(talons_rtc::decodeHour(0x91, false, false) == 23,
          "11 PM maps to hour 23");

    // ---- Range rejection ----------------------------------------
    check(talons_rtc::toUnixTime(2024, 13, 1, 0, 0, 0) == 0,
          "Invalid month rejected");
    check(talons_rtc::toUnixTime(2024, 1, 32, 0, 0, 0) == 0,
          "Invalid day rejected");
    check(talons_rtc::toUnixTime(2024, 1, 1, 24, 0, 0) == 0,
          "Invalid hour rejected");
    check(talons_rtc::toUnixTime(1960, 1, 1, 0, 0, 0) == 0,
          "Pre-epoch date rejected rather than wrapping");

    if (failures != 0)
    {
        std::printf("\n%d RTC test(s) FAILED\n", failures);
        return 1;
    }

    std::printf("\nAll RTC conversion tests passed\n");
    return 0;
}
