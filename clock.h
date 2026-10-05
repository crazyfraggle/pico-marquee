#ifndef CLOCK_H
#define CLOCK_H

#include <stdbool.h>
#include <stdint.h>

// Wall clock kept as an offset from the crystal-driven microsecond timer.
// It is set either by SNTP (Pico W) or by the "k" command, and converted to
// local time using a fixed UTC offset plus the EU summer time rule.

// Set the current UTC time as seconds (and microseconds) since 1970.
// Also the SNTP callback, see lwipopts.h.
void clock_set_utc(uint32_t sec, uint32_t us);

// True once the time has been set by any source.
bool clock_is_set(void);

// Current local time of day. Returns false if the clock has not been set.
bool clock_local_time(int *hour, int *min, int *sec);

#endif /* CLOCK_H */
