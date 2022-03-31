#ifndef TIME_H
#define TIME_H

#include <stdint.h>

#include "fdt.h"

typedef struct {
    uint64_t seconds;
    uint64_t micros;
} time_t;

// init_time(fdt_t*, uint64_t) -> void
// Initialises the timebase frequency.
void init_time(fdt_t* fdt, uint64_t hartid_);

// get_time() -> time_t
// Gets the current time.
time_t get_time();

// get_sync() -> time_t
// Gets the last synchronised time.
time_t get_sync();

// set_next_time_interrupt(time_t) -> void
// Sets the next time interrupt.
void set_next_time_interrupt(time_t time);

// synchronise_time(uint64_t) -> void
// Synchronises the time register across harts.
void synchronise_time(uint64_t hartid);

#endif /* TIME_H */
