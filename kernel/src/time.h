#ifndef TIME_H
#define TIME_H

#include <stdint.h>

typedef uint64_t time_t;

// get_time() -> time_t
// Gets the current time.
time_t get_time();

// sbi_set_timer(unsigned long long) -> struct sbiret
// Sets the timer value.
struct sbiret sbi_set_timer(time_t stime_value);

#endif /* TIME_H */
