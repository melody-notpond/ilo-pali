#ifndef TIME_H
#define TIME_H

#include <stdint.h>

#include "fdt.h"

typedef struct {
    uint64_t seconds;
    uint64_t micros;
} time_t;

// init_time(fdt_t*) -> void
// Initialises the timebase frequency.
void init_time(fdt_t* fdt);

// get_time() -> time_t
// Gets the current time.
time_t get_time();

// set_next_time_interrupt(time_t) -> void
// Sets the next time interrupt.
void set_next_time_interrupt(time_t time);

#endif /* TIME_H */
