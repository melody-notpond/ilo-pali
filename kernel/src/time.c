#include "time.h"

// get_time() -> time_t
// Gets the current time.
time_t get_time() {
    time_t time;
    asm volatile("rdtime %0" : "=r" (time));
    return time;
}
