#include <stdatomic.h>
#include <stddef.h>

#include "fdt.h"
#include "iter/string.h"
#include "iter/vec.h"
#include "sync.h"
#include "syscalls.h"
#include "join.h"
#include "fat16.h"
#include "format.h"

void thread() {
    uart_printf("thread started\n");
    for (int i = 0; i < 4; i++) {
        uart_printf("uwu\n");
        sleep(0, 500000);
    }

    exit(69);
}

void _start() {
    asm volatile(
        ".option push\n"
        ".option norelax\n"
        "lla gp, __global_pointer$\n"
        ".option pop\n"
    );

    uart_printf("uwud started\n");
    capability_t cap;
    spawn_thread(thread, NULL, 0, &cap);
    uart_printf("spawned thread\n");
    //send(true, cap, 4, 69, 0);
    uint64_t exit_code;
    if (!recv(true, cap, NULL, NULL, &exit_code, NULL)) {
        uart_printf("killed thread with exit code %i\n", exit_code);
    } else {
        uart_printf("connection closed\n");
    }

    while(1);
}

