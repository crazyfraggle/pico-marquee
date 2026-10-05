/*
 * Wall clock for the LED marquee.
 *
 * The RP2040 microsecond timer runs off the 12 MHz crystal, which is good to
 * a few seconds a day. Setting the clock just records the difference between
 * that timer and UTC; SNTP refreshes it hourly on a Pico W.
 *
 * Local time is UTC + CLOCK_UTC_OFFSET_MIN, plus one hour during EU summer
 * time (last Sunday of March to last Sunday of October, switching at 01:00
 * UTC) when CLOCK_EU_DST is non-zero. Both are set in CMakeLists.txt.
 */

#include "clock.h"

#include "pico/time.h"

#ifndef CLOCK_UTC_OFFSET_MIN
#define CLOCK_UTC_OFFSET_MIN 60
#endif
#ifndef CLOCK_EU_DST
#define CLOCK_EU_DST 1
#endif

static bool clock_valid = false;
// UTC in microseconds since 1970 = time_us_64() + utc_offset_us.
static int64_t utc_offset_us;

void clock_set_utc(uint32_t sec, uint32_t us)
{
    utc_offset_us = (int64_t)sec * 1000000 + us - (int64_t)time_us_64();
    clock_valid = true;
}

bool clock_is_set(void)
{
    return clock_valid;
}

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's
// days_from_civil).
static int64_t days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int yoe = (int)(y - era * 400);
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

// Calendar year containing the given day number (inverse of the above).
static int year_from_days(int64_t z)
{
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int doe = (int)(z - era * 146097);
    int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int mp = (5 * doy + 2) / 153;
    int m = mp < 10 ? mp + 3 : mp - 9;
    return (int)(yoe + era * 400) + (m <= 2);
}

// 01:00 UTC on the last Sunday of a 31-day month, in seconds since 1970.
static int64_t last_sunday_0100_utc(int year, int month)
{
    int64_t days = days_from_civil(year, month, 31);
    int weekday = (int)(((days + 4) % 7 + 7) % 7); // 1970-01-01 was a Thursday
    return (days - weekday) * 86400 + 3600;
}

static bool is_eu_summer_time(int64_t utc)
{
    int year = year_from_days(utc / 86400);
    return utc >= last_sunday_0100_utc(year, 3) &&
           utc < last_sunday_0100_utc(year, 10);
}

bool clock_local_time(int *hour, int *min, int *sec)
{
    if (!clock_valid)
    {
        return false;
    }

    int64_t utc = ((int64_t)time_us_64() + utc_offset_us) / 1000000;
    int64_t local = utc + CLOCK_UTC_OFFSET_MIN * 60;
    if (CLOCK_EU_DST && is_eu_summer_time(utc))
    {
        local += 3600;
    }

    int day_sec = (int)(((local % 86400) + 86400) % 86400);
    *hour = day_sec / 3600;
    *min = day_sec / 60 % 60;
    *sec = day_sec % 60;
    return true;
}
