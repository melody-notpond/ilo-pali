#include <stddef.h>

#include "console.h"
#include "opensbi.h"
#include "time.h"

uint64_t timebase;

// init_time(fdt_t*) -> void
// Initialises the timebase frequency.
void init_time(fdt_t* fdt) {
    void* cpus = fdt_path(fdt, "/cpus", NULL);
    struct fdt_property timebase_prop = fdt_get_property(fdt, cpus, "timebase-frequency");
    timebase = be_to_le(timebase_prop.len * 8, timebase_prop.data);
    time_t time = get_time();
    console_printf("[init_time] timebase set to %lx\ncurrent time is %lx::%lx\n", timebase, time.seconds, time.micros);
}

// get_time() -> time_t
// Gets the current time.
time_t get_time() {
    uint64_t time;
    asm volatile("csrr %0, time" : "=r" (time));
    return (time_t) {
        .seconds = time / timebase,
        .micros = (time % timebase) * 1000 * 1000 / timebase,
    };
}

// set_next_time_interrupt(time_t) -> void
// Sets the next time interrupt.
void set_next_time_interrupt(time_t time) {
    sbi_set_timer(time.seconds * timebase + time.micros * timebase / 1000 / 1000);
}
